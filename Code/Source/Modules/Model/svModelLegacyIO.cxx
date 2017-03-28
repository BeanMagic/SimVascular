#include "svModelLegacyIO.h"
#include "simvascular_options.h"

#include "svModel.h"
#include "svModelElementPolyData.h"
#include "cv_polydatasolid_utils.h"

#ifdef SV_USE_OpenCASCADE_QT_GUI
#include "svModelElementOCCT.h"
#endif

#ifdef SV_USE_PARASOLID_QT_GUI
#include "svModelElementParasolid.h"
#endif

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

mitk::DataNode::Pointer svModelLegacyIO::ReadFile(QString filePath)
{
    mitk::DataNode::Pointer modelNode=NULL;

    QFileInfo fileInfo(filePath);
    QString baseName = fileInfo.baseName();
    QString suffix=fileInfo.suffix();
    QString filePath2=filePath+".facenames";

    bool modelTypeDecided=false;
    QString handler="";
    if(suffix=="vtp" ||suffix=="vtk" || suffix=="stl" || suffix=="ply")
    {
        handler="set gPolyDataFaceNames(";

        std::vector<svModelElement::svFace*> faces;

        QFile inputFile(filePath2);
        if (inputFile.open(QIODevice::ReadOnly))
        {
            QTextStream in(&inputFile);

            while (!in.atEnd())
            {
                QString line = in.readLine();

                if(line.contains(handler))
                {
                    modelTypeDecided=true;

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
        else
        {
            //no .facenames, assume it's vtp model for stl file
            modelTypeDecided=true;
        }

        vtkSmartPointer<vtkPolyData> pd1=vtkSmartPointer<vtkPolyData>::New();
        if(PlyDtaUtils_ReadNative(const_cast<char*>(filePath.toStdString().c_str()),pd1) != SV_OK)
        {
          return modelNode;
        }

        vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
        cleaner->SetInputData(pd1);
        cleaner->Update();
        vtkSmartPointer<vtkPolyData> pd=cleaner->GetOutput();
        pd->BuildLinks();

        if(pd!=NULL)
        {
            svModelElementPolyData* mepd=new svModelElementPolyData();
            mepd->SetWholeVtkPolyData(pd);

            for(int i=0;i<faces.size();i++)
            {
                vtkSmartPointer<vtkPolyData> facepd=mepd->CreateFaceVtkPolyData(faces[i]->id);
                faces[i]->vpd=facepd;
            }

            mepd->SetFaces(faces);

            svModel::Pointer model=svModel::New();
            model->SetType(mepd->GetType());
            model->SetModelElement(mepd);
            model->SetDataModified();

            modelNode = mitk::DataNode::New();
            modelNode->SetData(model);
            modelNode->SetName(baseName.toStdString());
        }
    }

#ifdef SV_USE_OpenCASCADE_QT_GUI
    if(!modelTypeDecided && (suffix=="brep" || suffix=="step" || suffix=="stl" || suffix=="iges"))
    {
        handler="set gOCCTFaceNames(";

        std::vector<svModelElement::svFace*> faces;

        QFile inputFile(filePath2);
        if (inputFile.open(QIODevice::ReadOnly))
        {
            QTextStream in(&inputFile);

            while (!in.atEnd())
            {
                QString line = in.readLine();

                if(line.contains(handler))
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

        cvOCCTSolidModel* occtSolid=new cvOCCTSolidModel();
        occtSolid->ReadNative(const_cast<char*>(filePath.toStdString().c_str()));
        if(occtSolid)
        {
            svModelElementOCCT* meocct=new svModelElementOCCT();
            meocct->SetInnerSolid(occtSolid);
            meocct->SetWholeVtkPolyData(meocct->CreateWholeVtkPolyData());

            for(int i=0;i<faces.size();i++)
            {
                vtkSmartPointer<vtkPolyData> facepd=meocct->CreateFaceVtkPolyData(faces[i]->id);
                faces[i]->vpd=facepd;
            }

            meocct->SetFaces(faces);

            svModel::Pointer model=svModel::New();
            model->SetType(meocct->GetType());
            model->SetModelElement(meocct);
            model->SetDataModified();

            modelNode = mitk::DataNode::New();
            modelNode->SetData(model);
            modelNode->SetName(baseName.toStdString());
        }
    }
#endif

#ifdef SV_USE_PARASOLID_QT_GUI
    if(!modelTypeDecided && suffix=="xmt_txt")
    {
        cvParasolidSolidModel* parasolid=new cvParasolidSolidModel();
        char* fpath=const_cast<char*>(filePath.toStdString().c_str());
        parasolid->ReadNative(fpath);

        svModelElementParasolid* meps=new svModelElementParasolid();
        meps->SetInnerSolid(parasolid);
        meps->SetWholeVtkPolyData(meps->CreateWholeVtkPolyData());

        int numFaces;
        int *ids;
        int status=parasolid->GetFaceIds( &numFaces, &ids);

        std::vector<svModelElement::svFace*> faces;
        for(int i=0;i<numFaces;i++)
        {
            vtkSmartPointer<vtkPolyData> facevpd=meps->CreateFaceVtkPolyData(ids[i]);

            svModelElement::svFace* face =new svModelElement::svFace;
            char *value;
            parasolid->GetFaceAttribute("gdscName",ids[i],&value);
            std::string name(value);
            face->id=ids[i];
            face->name=name;
            face->vpd=facevpd;

            if(face->name.substr(0,5)=="wall_")
                face->type="wall";
            else
                face->type="cap";

            faces.push_back(face);
        }

        meps->SetFaces(faces);

        svModel::Pointer model=svModel::New();
        model->SetType(meps->GetType());
        model->SetModelElement(meps);
        model->SetDataModified();

        modelNode = mitk::DataNode::New();
        modelNode->SetData(model);
        modelNode->SetName(baseName.toStdString());
    }
#endif

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
                    out<<"set gPolyDataFaceNames("<<face->id<<") {"<<QString::fromStdString(face->name)<<"}"<<endl;
                }
            }

            outputFile.close();
        }
        else
        {
            return;
        }
    }

    if(type=="PolyData")
    {
        svModelElementPolyData* mepd=dynamic_cast<svModelElementPolyData*>(modelElement);
        if(!mepd) return;

        if(mepd->GetWholeVtkPolyData())
        {
            if (PlyDtaUtils_WriteNative(mepd->GetWholeVtkPolyData(), 0, const_cast<char*>(filePath.toStdString().c_str()) ) != SV_OK) {
                mitkThrow() << "PolyData model writing error.";
            }
        }
    }
#ifdef SV_USE_OpenCASCADE_QT_GUI
    else if(type=="OpenCASCADE")
    {
        svModelElementOCCT* meocct=dynamic_cast<svModelElementOCCT*>(modelElement);
        if(!meocct) return;

        if(meocct->GetInnerSolid())
        {
            char* fpath=const_cast<char*>(filePath.toStdString().c_str());
            if (meocct->GetInnerSolid()->WriteNative(0,fpath) != SV_OK )
             {
                 mitkThrow() << "OpenCASCADE model writing error.";
             }
        }
    }
#endif
#ifdef SV_USE_PARASOLID_QT_GUI
    else if(type=="Parasolid")
    {
        svModelElementParasolid* meps=dynamic_cast<svModelElementParasolid*>(modelElement);
        if(!meps) return;

        if(meps->GetInnerSolid())
        {
            char* fpath=const_cast<char*>(filePath.toStdString().c_str());
            if (meps->GetInnerSolid()->WriteNative(0,fpath) != SV_OK )
             {
                 mitkThrow() << "Parasolid model writing error.";
             }
        }
    }
#endif
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

        std::string type=modelElement->GetType();
        QString suffix;

        if(type=="PolyData")
            suffix="vtp";
        else if(type=="ParaSolid")
            suffix="xmt_txt";
        else if(type=="OpenCASCADE")
            suffix="brep";

        QString	filePath=dirModel.absoluteFilePath(QString::fromStdString(node->GetName())+"."+suffix);
        WriteFile(node, filePath);
    }

}
