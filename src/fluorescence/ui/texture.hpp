#ifndef FLUO_UI_TEXTURE_HPP
#define FLUO_UI_TEXTURE_HPP

#include <ClanLib/Display/Image/pixel_buffer.h>
#include <ClanLib/Display/Image/texture_format.h>
#include <ClanLib/Display/Render/texture.h>

#include <boost/shared_ptr.hpp>

#include "bitmask.hpp"
#include <data/ondemandreadable.hpp>

namespace fluo {
namespace ui {

class Texture : public fluo::data::OnDemandReadable {
public:
    Texture(bool useBitMask = true);

    void initPixelBuffer(unsigned int width, unsigned int height);
    uint32_t* getPixelBufferData();

    boost::shared_ptr<CL_PixelBuffer> getPixelBuffer();

    boost::shared_ptr<CL_Texture> getTexture();
    void setTexture(CL_PixelBuffer& pixBuf);

    unsigned int getWidth();
    unsigned int getHeight();

    bool hasPixel(unsigned int pixelX, unsigned int pixelY);

private:
    boost::shared_ptr<CL_PixelBuffer> pixelBuffer_;
    boost::shared_ptr<CL_Texture> texture_;
    bool useBitMask_;
    BitMask bitMask_;
};
}
}

#endif
