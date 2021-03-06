#include "svModelLegacyIO.h"

#include "svModel.h"
#include "svModelElementAnalytic.h"
#include "svModelElementFactory.h"

#include <QString>
#include <QStringList>
#include <QList>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>

#include <vtkXMLPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkErrorCode.h>

mitk::DataNode::Pointer svModelLegacyIO::ReadFile(QString filePath, QString preferredType)
{
    mitk::DataNode::Pointer modelNode=NULL;

    QFileInfo fileInfo(filePath);
    QString baseName = fileInfo.baseName();
    QString suffix=fileInfo.suffix();
    QString filePath2=filePath+".facenames";

    std::vector<svModelElement::svFace*> faces;
    std::string modelType="";

    QString handlerPolyData="set gPolyDataFaceNames(";
    QString handlerOCCT="set gOCCTFaceNames(";

    QFile inputFile(filePath2);
    if (inputFile.open(QIODevice::ReadOnly))
    {
        QTextStream in(&inputFile);

        while (!in.atEnd())
        {
            QString line = in.readLine();

            if(line.contains(handlerPolyData))
                modelType="PolyData";
            else if(line.contains(handlerOCCT))
                modelType="OpenCASCADE";

            //dont get face info for stl, ply, since the model has not face id.
            if(suffix=="stl" || suffix=="ply")
                continue;

            if(line.contains(handlerPolyData) || line.contains(handlerOCCT))
            {
                QStringList list = line.split(QRegExp("[(),{}\\s+]"), QString::SkipEmptyParts);
                svModelElement::svFace* face=new svModelElement::svFace;
                face->id=list[2].toInt();
                face->name=list[3].toStdString();
                faces.push_back(face);
                if(face->name.substr(0,4)=="wall")
                    face->type="wall";
                else if(face->name.substr(0,3)=="cap")
                    face->type="cap";
            }
        }
        inputFile.close();
    }

    if(preferredType=="")
    {
        if(suffix=="stl")
            modelType="PolyData";

        if(modelType=="")
            modelType=svModelElementFactory::GetType(suffix.toStdString());
    }
    else
        modelType=preferredType.toStdString();

    svModelElement* me=svModelElementFactory::CreateModelElement(modelType);
    if(me==NULL)
        return modelNode;

    if(!me->ReadFile(filePath.toStdString()))
    {
        delete me;
        return modelNode;
    }

    svModelElementAnalytic* meAnalytic=dynamic_cast<svModelElementAnalytic*>(me);
    if(meAnalytic)
        meAnalytic->SetWholeVtkPolyData(meAnalytic->CreateWholeVtkPolyData());

    if(faces.size()>0)//face info exists
    {
        for(int i=0;i<faces.size();i++)
            faces[i]->vpd=me->CreateFaceVtkPolyData(faces[i]->id);
    }
    else if(suffix!="stl" && suffix!="ply") //face info missing or embeded in model
    {
        std::vector<int> faceIDs=me->GetFaceIDsFromInnerSolid();
        for(int i=0;i<faceIDs.size();i++)
        {
            svModelElement::svFace* face =new svModelElement::svFace;

            face->id=faceIDs[i];
            if(modelType=="Parasolid")
                face->name=me->GetFaceNameFromInnerSolid(face->id);
            else
                face->name="noname_"+std::to_string(face->id);

            face->vpd=me->CreateFaceVtkPolyData(face->id);

            if(face->name.substr(0,5)=="wall_")
                face->type="wall";
            else if(face->name.substr(0,7)!="noname_")
                face->type="cap";

            faces.push_back(face);
        }
    }

    me->SetFaces(faces);

    svModel::Pointer model=svModel::New();
    model->SetType(me->GetType());
    model->SetModelElement(me);
    model->SetDataModified();

    modelNode = mitk::DataNode::New();
    modelNode->SetData(model);
    modelNode->SetName(baseName.toStdString());

    return modelNode;
}

std::vector<mitk::DataNode::Pointer> svModelLegacyIO::ReadFiles(QString modelDir)
{
    QDir dirModel(modelDir);
    QFileInfoList fileInfoList=dirModel.entryInfoList(QStringList("*"), QDir::Files, QDir::Name);
    std::vector<mitk::DataNode::Pointer> nodes;
    for(int i=0;i<fileInfoList.size();i++)
    {
        QString filePath=fileInfoList[i].absoluteFilePath();
        if(filePath.endsWith(".vtp"))
        {
            mitk::DataNode::Pointer node=ReadFile(filePath);
            if(node.IsNotNull())
                nodes.push_back(node);
        }
    }
    return nodes;
}

void svModelLegacyIO::WriteFile(mitk::DataNode::Pointer node, QString filePath)
{
    if(!node) return;

    svModel* model=dynamic_cast<svModel*>(node->GetData());
    if(!model) return;

    svModelElement* modelElement=model->GetModelElement();
    if(!modelElement) return;

    std::string type=modelElement->GetType();
    QString filePath2=filePath+".facenames";


    QString handler="";
    if(type=="PolyData")
        handler="gPolyDataFaceNames";
    else if(type=="OpenCASCADE")
        handler="gOCCTFaceNames";
    else if(type=="Parasolid")
        handler="";
    else
    {
        mitkThrow() << "Model type not support ";
        return;
    }

    if(handler!="")
    {
        QFile outputFile(filePath2);
        if(outputFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&outputFile);
            //out.setRealNumberPrecision(17);

            QFileInfo fileInfo(filePath);
            QString fileName = fileInfo.fileName();

            out<<"global "<<handler<<endl;
            out<<"global "<<handler<<"Info"<<endl;

            out<<"set "<<handler<<"Info(timestamp) {1500000000}"<<endl;
            //out<<"set "<<handler<<"Info(model_file_md5) {FCCF69239480A681C5579A153F2D552A}"<<endl;
            out<<"set "<<handler<<"Info(model_file_name) {"<<fileName<<"}"<<endl;

            std::vector<svModelElement::svFace*> faces=modelElement->GetFaces();
            for(int i=0;i<faces.size();i++){
                svModelElement::svFace* face=faces[i];
                if(face)
                {
                    out<<"set "<< handler<<"("<<face->id<<") {"<<QString::fromStdString(face->name)<<"}"<<endl;
                }
            }

            outputFile.close();
        }
//        else
//        {
//            return;
//        }
    }

    if(!modelElement->WriteFile(filePath.toStdString()))
        mitkThrow() << "Faied to write model to file: "<<filePath.toStdString();
}

void svModelLegacyIO::WriteFiles(mitk::DataStorage::SetOfObjects::ConstPointer modelNodes, QString modelDir)
{
    QDir dirModel(modelDir);

    for(int i=0;i<modelNodes->size();i++){
        mitk::DataNode::Pointer node=modelNodes->GetElement(i);

        if(!node) continue;

        svModel* model=dynamic_cast<svModel*>(node->GetData());
        if(!model) continue;

        svModelElement* modelElement=model->GetModelElement();
        if(!modelElement) continue;

        QString suffix="";
        auto exts=modelElement->GetFileExtensions();
        if(exts.size()>0)
            suffix=QString::fromStdString(exts[0]);

        QString	filePath=dirModel.absoluteFilePath(QString::fromStdString(node->GetName())+"."+suffix);
        WriteFile(node, filePath);
    }

}
