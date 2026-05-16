#pragma once

#include <QUndoCommand>

#include <osg/ref_ptr>
#include <osg/Node>
#include <osg/StateSet>
#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>
#include <osg/Material>
#include <osg/Texture2D>

// ============================================================================
// Transform Command - undo/redo for MatrixTransform
// ============================================================================
class TransformCommand : public QUndoCommand
{
public:
    TransformCommand(osg::MatrixTransform* node, const osg::Matrix& newMatrix,
                     QUndoCommand* parent = nullptr)
        : QUndoCommand(parent)
        , m_node(node)
        , m_oldMatrix(node->getMatrix())
        , m_newMatrix(newMatrix)
    {
        setText(QObject::tr("Change Transform"));
    }

    void undo() override
    {
        if (m_node.valid()) m_node->setMatrix(m_oldMatrix);
    }

    void redo() override
    {
        if (m_node.valid()) m_node->setMatrix(m_newMatrix);
    }

private:
    osg::observer_ptr<osg::MatrixTransform> m_node;
    osg::Matrix m_oldMatrix;
    osg::Matrix m_newMatrix;
};

// ============================================================================
// PAT Command - undo/redo for PositionAttitudeTransform
// ============================================================================
class PATCommand : public QUndoCommand
{
public:
    PATCommand(osg::PositionAttitudeTransform* node,
               const osg::Vec3& newPos, const osg::Quat& newAtt, const osg::Vec3& newScale,
               QUndoCommand* parent = nullptr)
        : QUndoCommand(parent)
        , m_node(node)
        , m_oldPos(node->getPosition())
        , m_oldAtt(node->getAttitude())
        , m_oldScale(node->getScale())
        , m_newPos(newPos)
        , m_newAtt(newAtt)
        , m_newScale(newScale)
    {
        setText(QObject::tr("Change Transform"));
    }

    void undo() override
    {
        if (m_node.valid())
        {
            m_node->setPosition(m_oldPos);
            m_node->setAttitude(m_oldAtt);
            m_node->setScale(m_oldScale);
        }
    }

    void redo() override
    {
        if (m_node.valid())
        {
            m_node->setPosition(m_newPos);
            m_node->setAttitude(m_newAtt);
            m_node->setScale(m_newScale);
        }
    }

private:
    osg::observer_ptr<osg::PositionAttitudeTransform> m_node;
    osg::Vec3 m_oldPos, m_newPos;
    osg::Quat m_oldAtt, m_newAtt;
    osg::Vec3 m_oldScale, m_newScale;
};

// ============================================================================
// Material Color Command - undo/redo for material color changes
// ============================================================================
class MaterialColorCommand : public QUndoCommand
{
public:
    // component: 0=ambient, 1=diffuse, 2=specular, 3=emission
    MaterialColorCommand(osg::Node* node, int component,
                         const osg::Vec4& oldColor, const osg::Vec4& newColor,
                         QUndoCommand* parent = nullptr)
        : QUndoCommand(parent)
        , m_node(node)
        , m_component(component)
        , m_oldColor(oldColor)
        , m_newColor(newColor)
    {
        const char* names[] = { "Ambient", "Diffuse", "Specular", "Emission" };
        setText(QObject::tr("Change %1 Color").arg(names[component]));
    }

    void undo() override
    {
        applyColor(m_oldColor);
    }

    void redo() override
    {
        applyColor(m_newColor);
    }

private:
    void applyColor(const osg::Vec4& color)
    {
        if (!m_node.valid()) return;
        osg::StateSet* ss = m_node->getOrCreateStateSet();
        osg::Material* mat = dynamic_cast<osg::Material*>(ss->getAttribute(osg::StateAttribute::MATERIAL));
        if (!mat)
        {
            mat = new osg::Material;
            ss->setAttributeAndModes(mat, osg::StateAttribute::ON);
        }
        switch (m_component)
        {
        case 0: mat->setAmbient(osg::Material::FRONT_AND_BACK, color); break;
        case 1: mat->setDiffuse(osg::Material::FRONT_AND_BACK, color); break;
        case 2: mat->setSpecular(osg::Material::FRONT_AND_BACK, color); break;
        case 3: mat->setEmission(osg::Material::FRONT_AND_BACK, color); break;
        }
    }

    osg::observer_ptr<osg::Node> m_node;
    int m_component;
    osg::Vec4 m_oldColor;
    osg::Vec4 m_newColor;
};

// ============================================================================
// Texture Command - undo/redo for texture changes
// ============================================================================
class TextureCommand : public QUndoCommand
{
public:
    TextureCommand(osg::Node* node, osg::Texture2D* oldTex, osg::Texture2D* newTex,
                   QUndoCommand* parent = nullptr)
        : QUndoCommand(parent)
        , m_node(node)
        , m_oldTexture(oldTex)
        , m_newTexture(newTex)
    {
        setText(newTex ? QObject::tr("Change Texture") : QObject::tr("Remove Texture"));
    }

    void undo() override
    {
        applyTexture(m_oldTexture.get());
    }

    void redo() override
    {
        applyTexture(m_newTexture.get());
    }

private:
    void applyTexture(osg::Texture2D* tex)
    {
        if (!m_node.valid()) return;
        osg::StateSet* ss = m_node->getOrCreateStateSet();
        if (tex)
        {
            ss->setTextureAttributeAndModes(0, tex, osg::StateAttribute::ON);
        }
        else
        {
            ss->removeTextureAttribute(0, osg::StateAttribute::TEXTURE);
        }
    }

    osg::observer_ptr<osg::Node> m_node;
    osg::ref_ptr<osg::Texture2D> m_oldTexture;
    osg::ref_ptr<osg::Texture2D> m_newTexture;
};

// ============================================================================
// Node Visibility Command
// ============================================================================
class VisibilityCommand : public QUndoCommand
{
public:
    VisibilityCommand(osg::Node* node, bool visible, QUndoCommand* parent = nullptr)
        : QUndoCommand(parent)
        , m_node(node)
        , m_oldMask(node ? node->getNodeMask() : 0xFFFFFFFF)
        , m_newMask(visible ? 0xFFFFFFFF : 0x0)
    {
        setText(visible ? QObject::tr("Show Node") : QObject::tr("Hide Node"));
    }

    void undo() override
    {
        if (m_node.valid()) m_node->setNodeMask(m_oldMask);
    }

    void redo() override
    {
        if (m_node.valid()) m_node->setNodeMask(m_newMask);
    }

private:
    osg::observer_ptr<osg::Node> m_node;
    unsigned int m_oldMask;
    unsigned int m_newMask;
};

// ============================================================================
// Wrap Node Command - auto-wrap a node with a MatrixTransform
// ============================================================================
class WrapNodeCommand : public QUndoCommand
{
public:
    WrapNodeCommand(osg::Group* parent, osg::Node* child, QUndoCommand* parentCmd = nullptr)
        : QUndoCommand(parentCmd)
        , m_parent(parent)
        , m_child(child)
        , m_wrapper(new osg::MatrixTransform)
        , m_index(0)
    {
        if (parent && child)
        {
            for (unsigned int i = 0; i < parent->getNumChildren(); ++i)
            {
                if (parent->getChild(i) == child)
                {
                    m_index = i;
                    break;
                }
            }
        }
        // Preserve the original node's name
        m_wrapper->setName(child->getName());
        child->setName(std::string());
        setText(QObject::tr("Wrap with Transform"));
    }

    osg::MatrixTransform* getWrapper() const { return m_wrapper.get(); }

    void undo() override
    {
        if (!m_parent.valid() || !m_child.valid() || !m_wrapper.valid()) return;
        // Restore original name
        m_child->setName(m_wrapper->getName());
        m_wrapper->setName(std::string());
        // Remove wrapper from parent, put child back
        m_wrapper->removeChild(m_child.get());
        m_parent->replaceChild(m_wrapper.get(), m_child.get());
    }

    void redo() override
    {
        if (!m_parent.valid() || !m_child.valid() || !m_wrapper.valid()) return;
        // Move name from child to wrapper
        m_wrapper->setName(m_child->getName());
        m_child->setName(std::string());
        // Add child to wrapper, replace child in parent with wrapper
        m_wrapper->addChild(m_child.get());
        m_parent->replaceChild(m_child.get(), m_wrapper.get());
    }

private:
    osg::observer_ptr<osg::Group> m_parent;
    osg::ref_ptr<osg::Node> m_child;
    osg::ref_ptr<osg::MatrixTransform> m_wrapper;
    unsigned int m_index;
};

// ============================================================================
// Delete Node Command
// =============================================================================
class DeleteNodeCommand : public QUndoCommand
{
public:
    DeleteNodeCommand(osg::Group* parent, osg::Node* child, QUndoCommand* parentCmd = nullptr)
        : QUndoCommand(parentCmd)
        , m_parent(parent)
        , m_child(child)
        , m_index(0)
    {
        if (parent && child)
        {
            for (unsigned int i = 0; i < parent->getNumChildren(); ++i)
            {
                if (parent->getChild(i) == child)
                {
                    m_index = i;
                    break;
                }
            }
        }
        setText(QObject::tr("Delete Node"));
    }

    void undo() override
    {
        if (m_parent.valid() && m_child.valid())
        {
            m_parent->insertChild(m_index, m_child.get());
        }
    }

    void redo() override
    {
        if (m_parent.valid() && m_child.valid())
        {
            m_parent->removeChild(m_child.get());
        }
    }

private:
    osg::observer_ptr<osg::Group> m_parent;
    osg::ref_ptr<osg::Node> m_child;
    unsigned int m_index;
};

// ============================================================================
// Duplicate Node Command
// ============================================================================
class DuplicateNodeCommand : public QUndoCommand
{
public:
    DuplicateNodeCommand(osg::Group* parent, osg::Node* clone, QUndoCommand* parentCmd = nullptr)
        : QUndoCommand(parentCmd)
        , m_parent(parent)
        , m_clone(clone)
    {
        setText(QObject::tr("Duplicate Node"));
    }

    void undo() override
    {
        if (m_parent.valid() && m_clone.valid())
            m_parent->removeChild(m_clone.get());
    }

    void redo() override
    {
        if (m_parent.valid() && m_clone.valid())
            m_parent->addChild(m_clone.get());
    }

    osg::Node* getClone() const { return m_clone.get(); }

private:
    osg::observer_ptr<osg::Group> m_parent;
    osg::ref_ptr<osg::Node> m_clone;
};

// ============================================================================
// StateSet Command - undo/redo for material/texture state changes
// Saves a snapshot of the entire StateSet before and after modification.
// ============================================================================
class StateSetCommand : public QUndoCommand
{
public:
    StateSetCommand(osg::Node* node, osg::StateSet* oldStateSet,
                    const QString& description, QUndoCommand* parentCmd = nullptr)
        : QUndoCommand(parentCmd)
        , m_node(node)
        , m_oldStateSet(oldStateSet ? static_cast<osg::StateSet*>(oldStateSet->clone(osg::CopyOp::DEEP_COPY_ALL)) : nullptr)
    {
        setText(description);
    }

    void setNewStateSet(osg::StateSet* newStateSet)
    {
        m_newStateSet = newStateSet ? static_cast<osg::StateSet*>(newStateSet->clone(osg::CopyOp::DEEP_COPY_ALL)) : nullptr;
    }

    void undo() override
    {
        if (!m_node.valid()) return;
        applyStateSet(m_oldStateSet.get());
    }

    void redo() override
    {
        if (!m_node.valid()) return;
        applyStateSet(m_newStateSet.get());
    }

private:
    void applyStateSet(osg::StateSet* ss)
    {
        osg::StateSet* current = m_node->getStateSet();
        if (current)
            m_node->setStateSet(nullptr);
        if (ss)
            m_node->setStateSet(static_cast<osg::StateSet*>(ss->clone(osg::CopyOp::DEEP_COPY_ALL)));
    }

    osg::observer_ptr<osg::Node> m_node;
    osg::ref_ptr<osg::StateSet> m_oldStateSet;
    osg::ref_ptr<osg::StateSet> m_newStateSet;
};

// ============================================================================
// Texture Wrap/Filter Command - undo/redo for texture wrap/filter changes
// ============================================================================
class TextureWrapFilterCommand : public QUndoCommand
{
public:
    TextureWrapFilterCommand(osg::Texture2D* texture,
                             osg::Texture::WrapMode oldWrapS, osg::Texture::WrapMode oldWrapT,
                             osg::Texture::FilterMode oldMinFilter, osg::Texture::FilterMode oldMagFilter,
                             osg::Texture::WrapMode newWrapS, osg::Texture::WrapMode newWrapT,
                             osg::Texture::FilterMode newMinFilter, osg::Texture::FilterMode newMagFilter,
                             QUndoCommand* parentCmd = nullptr)
        : QUndoCommand(parentCmd)
        , m_texture(texture)
        , m_oldWrapS(oldWrapS), m_oldWrapT(oldWrapT)
        , m_oldMinFilter(oldMinFilter), m_oldMagFilter(oldMagFilter)
        , m_newWrapS(newWrapS), m_newWrapT(newWrapT)
        , m_newMinFilter(newMinFilter), m_newMagFilter(newMagFilter)
    {
        setText(QObject::tr("Change Texture Wrap/Filter"));
    }

    void undo() override
    {
        if (!m_texture.valid()) return;
        m_texture->setWrap(osg::Texture::WRAP_S, m_oldWrapS);
        m_texture->setWrap(osg::Texture::WRAP_T, m_oldWrapT);
        m_texture->setFilter(osg::Texture::MIN_FILTER, m_oldMinFilter);
        m_texture->setFilter(osg::Texture::MAG_FILTER, m_oldMagFilter);
    }

    void redo() override
    {
        if (!m_texture.valid()) return;
        m_texture->setWrap(osg::Texture::WRAP_S, m_newWrapS);
        m_texture->setWrap(osg::Texture::WRAP_T, m_newWrapT);
        m_texture->setFilter(osg::Texture::MIN_FILTER, m_newMinFilter);
        m_texture->setFilter(osg::Texture::MAG_FILTER, m_newMagFilter);
    }

private:
    osg::observer_ptr<osg::Texture2D> m_texture;
    osg::Texture::WrapMode m_oldWrapS, m_oldWrapT;
    osg::Texture::FilterMode m_oldMinFilter, m_oldMagFilter;
    osg::Texture::WrapMode m_newWrapS, m_newWrapT;
    osg::Texture::FilterMode m_newMinFilter, m_newMagFilter;
};
