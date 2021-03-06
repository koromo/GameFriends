#include "../audio/soundclip.h"
#include "logging.h"
#include "foundation/exception.h"
#include "foundation/string.h"

GF_NAMESPACE_BEGIN

bool SoundClip::loadImpl()
{
    const auto tpath = charset(osPath());

    const auto mmio = mmioOpen(const_cast<TCHAR*>(tpath.c_str()), nullptr, MMIO_READ);
    if (!mmio)
    {
        GF_LOG_WARN("Failed to load sound clip {}.", osPath());
        return false;
    }
    GF_SCOPE_EXIT{ mmioClose(mmio, 0); };

    MMRESULT mr;

    MMCKINFO riffChunk;
    riffChunk.fccType = mmioFOURCC('W', 'A', 'V', 'E');
    mr = mmioDescend(mmio, &riffChunk, nullptr, MMIO_FINDRIFF);
    check(mr == MMSYSERR_NOERROR);

    MMCKINFO fmtChunk;
    fmtChunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
    mr = mmioDescend(mmio, &fmtChunk, &riffChunk, MMIO_FINDCHUNK);
    check(mr == MMSYSERR_NOERROR);

    auto readSize = mmioRead(mmio, reinterpret_cast<LPSTR>(&format_), fmtChunk.cksize);
    check(readSize == fmtChunk.cksize);

    mr = mmioAscend(mmio, &fmtChunk, 0);
    check(mr == MMSYSERR_NOERROR);

    MMCKINFO dataChunk;
    dataChunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
    mr = mmioDescend(mmio, &dataChunk, &riffChunk, MMIO_FINDCHUNK);
    check(mr == MMSYSERR_NOERROR);

    const auto data = new char[dataChunk.cksize];
    data_.reset(reinterpret_cast<unsigned char*>(data));
    size_ = dataChunk.cksize;
    readSize = mmioRead(mmio, data, dataChunk.cksize);
    check(readSize == dataChunk.cksize);

    return true;
}

void SoundClip::unloadImpl()
{
    data_.reset();
    size_ = 0;
    format_ = {};
}

GF_NAMESPACE_END
