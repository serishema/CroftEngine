#include "Base.h"
#include "Text.h"
#include "Scene.h"
#include "Game.h"


namespace gameplay
{
    Text::Text() :
                 _font(nullptr)
                 , _text("")
                 , _size(0)
                 , _width(0)
                 , _height(0)
                 , _wrap(true)
                 , _rightToLeft(false)
                 , _align(Font::ALIGN_TOP_LEFT)
                 , _clip(Rectangle(0, 0, 0, 0))
                 , _opacity(1.0f)
                 , _color(Vector4::one())
    {
    }


    Text::~Text()
    {
        SAFE_RELEASE(_font);
    }


    Text& Text::operator=(const Text& text)
    {
        return *this;
    }


    void Text::setText(const char* str)
    {
        _text = str;
    }


    const char* Text::getText() const
    {
        return _text.c_str();
    }


    unsigned int Text::getSize() const
    {
        return _size;
    }


    void Text::setWidth(float width)
    {
        _width = width;
    }


    float Text::getWidth() const
    {
        return _width;
    }


    void Text::setHeight(float height)
    {
        _height = height;
    }


    float Text::getHeight() const
    {
        return _height;
    }


    void Text::setWrap(bool wrap)
    {
        _wrap = wrap;
    }


    bool Text::getWrap() const
    {
        return _wrap;
    }


    void Text::setRightToLeft(bool rightToLeft)
    {
        _rightToLeft = rightToLeft;
    }


    bool Text::getRightToLeft() const
    {
        return _rightToLeft;
    }


    void Text::setJustify(Font::Justify align)
    {
        _align = align;
    }


    Font::Justify Text::getJustify() const
    {
        return _align;
    }


    void Text::setClip(const Rectangle& clip)
    {
        _clip = clip;
    }


    const Rectangle& Text::getClip() const
    {
        return _clip;
    }


    void Text::setOpacity(float opacity)
    {
        _opacity = opacity;
    }


    float Text::getOpacity() const
    {
        return _opacity;
    }


    void Text::setColor(const Vector4& color)
    {
        _color = color;
    }


    const Vector4& Text::getColor() const
    {
        return _color;
    }


    unsigned int Text::draw(bool wireframe)
    {
        // Apply scene camera projection and translation offsets
        Rectangle viewport = Game::getInstance()->getViewport();
        Vector3 position = Vector3::zero();

        // Font is always using a offset projection matrix to top-left. So we need to adjust it back to cartesian
        position.x += viewport.width / 2;
        position.y += viewport.height / 2;
        Rectangle clipViewport = _clip;
        if( _node && _node->getScene() )
        {
            Camera* activeCamera = _node->getScene()->getActiveCamera();
            if( activeCamera )
            {
                Node* cameraNode = _node->getScene()->getActiveCamera()->getNode();
                if( cameraNode )
                {
                    // Camera translation offsets
                    position.x -= cameraNode->getTranslationWorld().x;
                    position.y += cameraNode->getTranslationWorld().y - getHeight();
                }
            }

            // Apply node translation offsets
            Vector3 translation = _node->getTranslationWorld();
            position.x += translation.x;
            position.y -= translation.y;

            if( !clipViewport.isEmpty() )
            {
                clipViewport.x += position.x;
                clipViewport.y += position.y;
            }
        }
        _font->start();
        _font->drawText(_text.c_str(), Rectangle(position.x, position.y, _width, _height),
                        Vector4(_color.x, _color.y, _color.z, _color.w * _opacity), _size,
                        _align, _wrap, _rightToLeft, clipViewport);
        _font->finish();
        return 1;
    }


    int Text::getPropertyId(TargetType type, const char* propertyIdStr)
    {
        GP_ASSERT(propertyIdStr);

        if( type == AnimationTarget::TRANSFORM )
        {
            if( strcmp(propertyIdStr, "ANIMATE_OPACITY") == 0 )
            {
                return Text::ANIMATE_OPACITY;
            }
            else if( strcmp(propertyIdStr, "ANIMATE_COLOR") == 0 )
            {
                return Text::ANIMATE_COLOR;
            }
        }

        return AnimationTarget::getPropertyId(type, propertyIdStr);
    }


    unsigned int Text::getAnimationPropertyComponentCount(int propertyId) const
    {
        switch( propertyId )
        {
            case ANIMATE_OPACITY:
                return 1;
            case ANIMATE_COLOR:
                return 4;
            default:
                return -1;
        }
    }


    void Text::getAnimationPropertyValue(int propertyId, AnimationValue* value)
    {
        GP_ASSERT(value);

        switch( propertyId )
        {
            case ANIMATE_OPACITY:
                value->setFloat(0, _opacity);
                break;
            case ANIMATE_COLOR:
                value->setFloat(0, _color.x);
                value->setFloat(1, _color.y);
                value->setFloat(2, _color.z);
                value->setFloat(3, _color.w);
                break;
            default:
                break;
        }
    }


    void Text::setAnimationPropertyValue(int propertyId, AnimationValue* value, float blendWeight)
    {
        GP_ASSERT(value);

        switch( propertyId )
        {
            case ANIMATE_OPACITY:
                setOpacity(Curve::lerp(blendWeight, _opacity, value->getFloat(0)));
                break;
            case ANIMATE_COLOR:
                setColor(Vector4(Curve::lerp(blendWeight, _color.x, value->getFloat(0)),
                                 Curve::lerp(blendWeight, _color.x, value->getFloat(1)),
                                 Curve::lerp(blendWeight, _color.x, value->getFloat(2)),
                                 Curve::lerp(blendWeight, _color.x, value->getFloat(3))));
                break;
            default:
                break;
        }
    }
}
