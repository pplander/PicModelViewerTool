#pragma once

#include <string>

#include <osg/ref_ptr>
#include <osg/Node>

struct aiScene;
struct aiNode;
struct aiMesh;
struct aiMaterial;

class AssimpToOSGConverter
{
public:
    AssimpToOSGConverter();
    ~AssimpToOSGConverter();

    osg::Node* convert(const aiScene* scene, const std::string& modelPath);

    int getMeshCount() const { return m_meshCount; }
    int getMaterialCount() const { return m_materialCount; }
    int getTextureCount() const { return m_textureCount; }

private:
    osg::Node* processNode(const aiScene* scene, const aiNode* node, const std::string& modelPath);
    osg::Geode* processMesh(const aiScene* scene, const aiMesh* mesh);
    void processMaterial(const aiScene* scene, const aiMaterial* material,
        osg::StateSet* stateSet, const std::string& modelPath);
    void applyMaterialToGeometry(osg::Geometry* geom, const aiScene* scene,
        unsigned int materialIndex, const std::string& modelPath);

    std::string resolveTexturePath(const std::string& textureFile, const std::string& modelPath);

    int m_meshCount = 0;
    int m_materialCount = 0;
    int m_textureCount = 0;
};
