#include "svModelElementOCCT.h"
#include "svModelUtilsOCCT.h"

#include <iostream>

svModelElementOCCT::svModelElementOCCT()
{
    m_Type="OpenCASCADE";
    m_MaxDist=20.0;
    std::vector<std::string> exts={"brep","step","iges","stl"};
    m_FileExtensions=exts;
}

svModelElementOCCT::svModelElementOCCT(const svModelElementOCCT &other)
    : svModelElementAnalytic(other)
{
}

svModelElementOCCT::~svModelElementOCCT()
{
}

svModelElementOCCT* svModelElementOCCT::Clone()
{
    return new svModelElementOCCT(*this);
}

svModelElement* svModelElementOCCT::CreateModelElement()
{
    return new svModelElementOCCT();
}

svModelElement* svModelElementOCCT::CreateModelElement(std::vector<mitk::DataNode::Pointer> segNodes
                                , int numSamplingPts
                                , svLoftingParam *param
                                , int* stats
                                , double maxDist
                                , int noInterOut
                                , double tol
                                , unsigned int t)
{
    return svModelUtilsOCCT::CreateModelElementOCCT(segNodes,numSamplingPts,param,maxDist,t);
}

svModelElement* svModelElementOCCT::CreateModelElementByBlend(std::vector<svModelElement::svBlendParamRadius*> blendRadii
                                                  , svModelElement::svBlendParam* param)
{
    return svModelUtilsOCCT::CreateModelElementOCCTByBlend(this,blendRadii);
}

bool svModelElementOCCT::ReadFile(std::string filePath)
{
    cvOCCTSolidModel* occtSolid=new cvOCCTSolidModel();
    char* df=const_cast<char*>(filePath.c_str());
    m_InnerSolid=occtSolid;
    if(m_InnerSolid->ReadNative(df)==SV_OK)
        return true;
    else
        return false;
}

bool svModelElementOCCT::WriteFile(std::string filePath)
{
    if(m_InnerSolid)
    {
        char* df=const_cast<char*>(filePath.c_str());
        if (m_InnerSolid->WriteNative(0,df) != SV_OK )
            return false;
    }

    return true;
}
