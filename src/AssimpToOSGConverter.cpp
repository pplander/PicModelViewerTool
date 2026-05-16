#include "AssimpToOSGConverter.h"

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/Material>
#include <osg/StateSet>
#include <osg/Texture2D>
#include <osg/TexEnv>
#include <osg/BlendFunc>
#include <osg/CullFace>
#include <osg/Depth>
#include <osg/PolygonMode>
#include <osgDB/ReadFile>
#include <osgDB/FileUtils>

#include <assimp/scene.h>
#include <assimp/material.h>
#include <assimp/mesh.h>

#include <algorithm>
#include <filesystem>

AssimpToOSGConverter::AssimpToOSGConverter()
{
}

AssimpToOSGConverter::~AssimpToOSGConverter()
{
}

osg::Node* AssimpToOSGConverter::convert(const aiScene* scene, const std::string& modelPath)
{
    if (!scene || !scene->mRootNode) return nullptr;

    m_meshCount = 0;
    m_materialCount = 0;
    m_textureCount = 0;

    return processNode(scene, scene->mRootNode, modelPath);
}

osg::Node* AssimpToOSGConverter::processNode(const aiScene* scene, const aiNode* node, const std::string& modelPath)
{
    // Create a group or transform
    osg::ref_ptr<osg::Group> group;

    // Check if we need a MatrixTransform
    if (node->mTransformation.IsIdentity())
    {
        group = new osg::Group;
    }
    else
    {
        osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform;

        // Convert aiMatrix4x4 to osg::Matrix
        aiMatrix4x4 m = node->mTransformation;
        osg::Matrixf osgMat(
            m.a1, m.b1, m.c1, m.d1,
            m.a2, m.b2, m.c2, m.d2,
            m.a3, m.b3, m.c3, m.d3,
            m.a4, m.b4, m.c4, m.d4
        );
        transform->setMatrix(osgMat);
        group = transform;
    }

    // Set node name
    if (node->mName.length > 0)
    {
        group->setName(node->mName.C_Str());
    }

    // Process meshes
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        unsigned int meshIndex = node->mMeshes[i];
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        if (mesh)
        {
            osg::Geode* geode = processMesh(scene, mesh);
            if (geode)
            {
                // Apply material
                if (mesh->mMaterialIndex < scene->mNumMaterials)
                {
                    osg::StateSet* ss = geode->getOrCreateStateSet();
                    processMaterial(scene, scene->mMaterials[mesh->mMaterialIndex], ss, modelPath);
                }
                group->addChild(geode);
            }
        }
    }

    // Process children
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        osg::Node* childNode = processNode(scene, node->mChildren[i], modelPath);
        if (childNode)
        {
            group->addChild(childNode);
        }
    }

    return group.release();
}

osg::Geode* AssimpToOSGConverter::processMesh(const aiScene* scene, const aiMesh* mesh)
{
    if (!mesh || mesh->mNumVertices == 0) return nullptr;

    m_meshCount++;

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;

    // Set geometry name
    if (mesh->mName.length > 0)
    {
        geom->setName(mesh->mName.C_Str());
    }

    // Vertices
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array(mesh->mNumVertices);
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
    {
        (*vertices)[i].set(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
    }
    geom->setVertexArray(vertices.get());

    // Normals
    if (mesh->HasNormals())
    {
        osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array(mesh->mNumVertices);
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            (*normals)[i].set(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        }
        geom->setNormalArray(normals.get(), osg::Array::BIND_PER_VERTEX);
    }

    // Texture coordinates (up to 8 UV sets)
    for (unsigned int uv = 0; uv < std::min(mesh->GetNumUVChannels(), 8u); ++uv)
    {
        osg::ref_ptr<osg::Vec2Array> texCoords = new osg::Vec2Array(mesh->mNumVertices);
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            if (mesh->mTextureCoords[uv])
            {
                (*texCoords)[i].set(mesh->mTextureCoords[uv][i].x, mesh->mTextureCoords[uv][i].y);
            }
            else
            {
                (*texCoords)[i].set(0.0f, 0.0f);
            }
        }
        geom->setTexCoordArray(uv, texCoords.get());
    }

    // Vertex colors
    for (unsigned int c = 0; c < std::min(mesh->GetNumColorChannels(), 8u); ++c)
    {
        if (mesh->mColors[c])
        {
            osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array(mesh->mNumVertices);
            for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
            {
                (*colors)[i].set(
                    mesh->mColors[c][i].r,
                    mesh->mColors[c][i].g,
                    mesh->mColors[c][i].b,
                    mesh->mColors[c][i].a
                );
            }
            geom->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
        }
    }

    // Indices (faces)
    if (mesh->HasFaces())
    {
        osg::ref_ptr<osg::DrawElementsUInt> indices = new osg::DrawElementsUInt(GL_TRIANGLES);
        indices->reserve(mesh->mNumFaces * 3);
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
        {
            const aiFace& face = mesh->mFaces[i];
            if (face.mNumIndices == 3)
            {
                indices->push_back(face.mIndices[0]);
                indices->push_back(face.mIndices[1]);
                indices->push_back(face.mIndices[2]);
            }
            else if (face.mNumIndices == 4)
            {
                // Triangulate quad
                indices->push_back(face.mIndices[0]);
                indices->push_back(face.mIndices[1]);
                indices->push_back(face.mIndices[2]);
                indices->push_back(face.mIndices[0]);
                indices->push_back(face.mIndices[2]);
                indices->push_back(face.mIndices[3]);
            }
            else
            {
                // Fan triangulation for polygons
                for (unsigned int j = 1; j < face.mNumIndices - 1; ++j)
                {
                    indices->push_back(face.mIndices[0]);
                    indices->push_back(face.mIndices[j]);
                    indices->push_back(face.mIndices[j + 1]);
                }
            }
        }
        geom->addPrimitiveSet(indices.get());
    }

    // Generate normals if not present
    if (!mesh->HasNormals())
    {
        geom->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
    }

    geode->addDrawable(geom.get());

    // Ensure lighting is on
    osg::StateSet* ss = geode->getOrCreateStateSet();
    ss->setMode(GL_LIGHTING, osg::StateAttribute::ON);

    return geode.release();
}

void AssimpToOSGConverter::processMaterial(const aiScene* scene, const aiMaterial* material,
    osg::StateSet* stateSet, const std::string& modelPath)
{
    m_materialCount++;

    if (!material) return;

    // Create OSG material
    osg::ref_ptr<osg::Material> mat = new osg::Material;

    // Ambient color
    aiColor4D ambient;
    if (material->Get(AI_MATKEY_COLOR_AMBIENT, ambient) == AI_SUCCESS)
    {
        mat->setAmbient(osg::Material::FRONT_AND_BACK,
            osg::Vec4(ambient.r, ambient.g, ambient.b, ambient.a));
    }
    else
    {
        mat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.2f, 0.2f, 0.2f, 1.0f));
    }

    // Diffuse color
    aiColor4D diffuse;
    if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS)
    {
        mat->setDiffuse(osg::Material::FRONT_AND_BACK,
            osg::Vec4(diffuse.r, diffuse.g, diffuse.b, diffuse.a));
    }
    else
    {
        mat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.8f, 0.8f, 0.8f, 1.0f));
    }

    // Specular color
    aiColor4D specular;
    if (material->Get(AI_MATKEY_COLOR_SPECULAR, specular) == AI_SUCCESS)
    {
        mat->setSpecular(osg::Material::FRONT_AND_BACK,
            osg::Vec4(specular.r, specular.g, specular.b, specular.a));
    }

    // Emissive color
    aiColor4D emissive;
    if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS)
    {
        mat->setEmission(osg::Material::FRONT_AND_BACK,
            osg::Vec4(emissive.r, emissive.g, emissive.b, emissive.a));
    }

    // Shininess
    float shininess;
    if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
    {
        mat->setShininess(osg::Material::FRONT_AND_BACK, shininess);
    }

    // Opacity / transparency
    float opacity = 1.0f;
    if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS && opacity < 1.0f)
    {
        mat->setAlpha(osg::Material::FRONT_AND_BACK, opacity);
        osg::ref_ptr<osg::BlendFunc> blend = new osg::BlendFunc;
        blend->setFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        stateSet->setAttributeAndModes(blend.get(), osg::StateAttribute::ON);
        stateSet->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
        // Disable depth writing for transparent objects
        osg::ref_ptr<osg::Depth> depth = new osg::Depth;
        depth->setWriteMask(false);
        stateSet->setAttributeAndModes(depth.get(), osg::StateAttribute::ON);
    }
    else
    {
        // Enable back-face culling for opaque objects
        osg::ref_ptr<osg::CullFace> cullFace = new osg::CullFace(osg::CullFace::BACK);
        stateSet->setAttributeAndModes(cullFace.get(), osg::StateAttribute::ON);
    }

    // Two-sided material
    int twoSided = 0;
    if (material->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS && twoSided)
    {
        stateSet->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
        mat->setColorMode(osg::Material::AMBIENT_AND_DIFFUSE);
    }

    stateSet->setAttributeAndModes(mat.get(), osg::StateAttribute::ON);

    // Diffuse texture
    if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
    {
        aiString texPath;
        if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
        {
            std::string textureFile = texPath.C_Str();
            std::string fullPath = resolveTexturePath(textureFile, modelPath);

            osg::ref_ptr<osg::Texture2D> texture = nullptr;

            // Check if it's an embedded texture (starts with *)
            if (textureFile.size() > 0 && textureFile[0] == '*')
            {
                // Embedded texture - extract from scene
                int texIndex = std::atoi(textureFile.c_str() + 1);
                if (scene->HasTextures() && texIndex < (int)scene->mNumTextures)
                {
                    const aiTexture* aiTex = scene->mTextures[texIndex];
                    // Create texture from embedded data
                    osg::ref_ptr<osg::Image> image = new osg::Image;
                    if (aiTex->mHeight == 0)
                    {
                        // Compressed texture (PNG/JPEG stored as raw bytes)
                        // mWidth = size of compressed data in bytes
                        // Try loading via temporary file approach
                        // Skip for now as osgDB::readImageFromMemory is not available
                        image = nullptr;
                    }
                    else
                    {
                        // Uncompressed RGBA pixel data
                        image->allocateImage(aiTex->mWidth, aiTex->mHeight, 1, GL_RGBA, GL_UNSIGNED_BYTE);
                        memcpy(image->data(), aiTex->pcData, aiTex->mWidth * aiTex->mHeight * 4);
                    }
                    if (image && image->valid())
                    {
                        texture = new osg::Texture2D(image.get());
                    }
                }
            }
            else
            {
                // External texture file
                osg::ref_ptr<osg::Image> image = osgDB::readImageFile(fullPath);
                if (!image)
                {
                    // Try without path
                    image = osgDB::readImageFile(textureFile);
                }
                if (image)
                {
                    texture = new osg::Texture2D(image.get());
                }
            }

            if (texture)
            {
                m_textureCount++;
                texture->setWrap(osg::Texture2D::WRAP_S, osg::Texture2D::REPEAT);
                texture->setWrap(osg::Texture2D::WRAP_T, osg::Texture2D::REPEAT);
                texture->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR_MIPMAP_LINEAR);
                texture->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
                stateSet->setTextureAttributeAndModes(0, texture.get(), osg::StateAttribute::ON);

                // Modulate mode for texture
                osg::ref_ptr<osg::TexEnv> texEnv = new osg::TexEnv(osg::TexEnv::MODULATE);
                stateSet->setTextureAttributeAndModes(0, texEnv.get(), osg::StateAttribute::ON);
            }
        }
    }

    // Normal map
    if (material->GetTextureCount(aiTextureType_NORMALS) > 0)
    {
        aiString texPath;
        if (material->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS)
        {
            std::string textureFile = texPath.C_Str();
            std::string fullPath = resolveTexturePath(textureFile, modelPath);
            osg::ref_ptr<osg::Image> image = osgDB::readImageFile(fullPath);
            if (image)
            {
                osg::ref_ptr<osg::Texture2D> normalMap = new osg::Texture2D(image.get());
                normalMap->setWrap(osg::Texture2D::WRAP_S, osg::Texture2D::REPEAT);
                normalMap->setWrap(osg::Texture2D::WRAP_T, osg::Texture2D::REPEAT);
                stateSet->setTextureAttributeAndModes(1, normalMap.get(), osg::StateAttribute::ON);
            }
        }
    }

    // Wireframe mode
    int wireframe = 0;
    if (material->Get(AI_MATKEY_ENABLE_WIREFRAME, wireframe) == AI_SUCCESS && wireframe)
    {
        osg::ref_ptr<osg::PolygonMode> pm = new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::LINE);
        stateSet->setAttributeAndModes(pm.get(), osg::StateAttribute::ON);
    }
}

void AssimpToOSGConverter::applyMaterialToGeometry(osg::Geometry* geom, const aiScene* scene,
    unsigned int materialIndex, const std::string& modelPath)
{
    if (!geom || materialIndex >= scene->mNumMaterials) return;

    osg::StateSet* ss = geom->getOrCreateStateSet();
    processMaterial(scene, scene->mMaterials[materialIndex], ss, modelPath);
}

std::string AssimpToOSGConverter::resolveTexturePath(const std::string& textureFile, const std::string& modelPath)
{
    // If absolute path, return as is
    if (textureFile.find(':') != std::string::npos || textureFile.find('/') == 0 || textureFile.find("\\") == 0)
    {
        return textureFile;
    }

    // Try relative to model directory
    std::filesystem::path modelDir = std::filesystem::path(modelPath).parent_path();
    std::filesystem::path fullPath = modelDir / textureFile;
    if (std::filesystem::exists(fullPath))
    {
        return fullPath.string();
    }

    // Try just the filename in the model directory
    std::filesystem::path texName = std::filesystem::path(textureFile).filename();
    fullPath = modelDir / texName;
    if (std::filesystem::exists(fullPath))
    {
        return fullPath.string();
    }

    // Return the path relative to model directory even if it doesn't exist
    return (modelDir / textureFile).string();
}
