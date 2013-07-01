/*
 * fluorescence is a free, customizable Ultima Online client.
 * Copyright (C) 2011-2012, http://fluorescence-client.org

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */



#include "manager.hpp"

#include <boost/filesystem/operations.hpp>
#include <unicode/regex.h>

#include "tiledataloader.hpp"
#include "artloader.hpp"
#include "huesloader.hpp"
#include "gumpartloader.hpp"
#include "maploader.hpp"
#include "staticsloader.hpp"
#include "maptexloader.hpp"
#include "animdataloader.hpp"
#include "animloader.hpp"
#include "mobtypesloader.hpp"
#include "unifontloader.hpp"
#include "deffileloader.hpp"
#include "equipconvdefloader.hpp"
#include "clilocloader.hpp"
#include "httploader.hpp"
#include "filepathloader.hpp"
#include "soundloader.hpp"
#include "spellbooks.hpp"
#include "skillsloader.hpp"
#include "radarcolloader.hpp"

#include <client.hpp>

#include <ui/manager.hpp>
#include <ui/singletextureprovider.hpp>
#include <ui/animdatatextureprovider.hpp>

#include <typedefs.hpp>

namespace fluo {
namespace data {

Manager* Manager::singleton_= NULL;

bool Manager::create() {
    if (!singleton_) {
        try {
            singleton_ = new Manager();
        } catch (const std::exception& ex) {
            LOG_EMERGENCY << "Error initializing data::Manager: " << ex.what() << std::endl;
            return false;
        }
    }

    return true;
}

void Manager::destroy() {
    if (singleton_) {
        delete singleton_;
        singleton_ = NULL;
    }
}

Manager* Manager::getSingleton() {
    if (!singleton_) {
        throw Exception("fluo::data::Manager Singleton not created yet");
    }

    return singleton_;
}

Manager::Manager() {
    LOG_INFO << "Initializing http loader" << std::endl;
    httpLoader_.reset(new HttpLoader());

    LOG_INFO << "Initializing file path loader" << std::endl;
    filePathLoader_.reset(new FilePathLoader());
}

bool Manager::setShardConfig(Config& config) {
    UnicodeString fileFormatStr = config["/fluo/files@format"].asString();
    if (fileFormatStr == "mul") {
        fileFormat_ = FileFormat::MUL;
    } else if (fileFormatStr == "mul-hs") {
        fileFormat_ = FileFormat::MUL_HIGH_SEAS;
    } else if (fileFormatStr == "uop") {
        fileFormat_ = FileFormat::UOP;
        LOG_ERROR << "UOP file format not qupported yet" << std::endl;
        return false;
    } else {
        LOG_ERROR << "Unsupported file format. Supported: \"mul\" for pre high seas files, \"mul-hs\" for high seas files" << std::endl;
        return false;
    }

    LOG_DEBUG << "Data file format: " << fileFormatStr << std::endl;

    buildFilePathMap(config);

    //std::map<std::string, boost::filesystem::path>::iterator iter = filePathMap_.begin();
    //std::map<std::string, boost::filesystem::path>::iterator end = filePathMap_.end();

    //for (; iter != end; ++iter) {
        //LOG_DEBUG << "Path for \"" << iter->first << "\" is " << iter->second << std::endl;
    //}


    boost::filesystem::path idxPath;
    boost::filesystem::path path;
    boost::filesystem::path difOffsetsPath;
    boost::filesystem::path difIdxPath;
    boost::filesystem::path difPath;

    checkFileExists("tiledata.mul");
    path = filePathMap_["tiledata.mul"];
    LOG_INFO << "Opening tiledata.mul from mul=" << path << std::endl;
    tileDataLoader_.reset(new TileDataLoader(path, fileFormat_ == FileFormat::MUL_HIGH_SEAS));

    checkFileExists("hues.mul");
    path = filePathMap_["hues.mul"];
    LOG_INFO << "Opening hues.mul from mul=" << path << std::endl;
    huesLoader_.reset(new HuesLoader(path));

    checkFileExists("texidx.mul");
    checkFileExists("texmaps.mul");
    idxPath = filePathMap_["texidx.mul"];
    path = filePathMap_["texmaps.mul"];
    LOG_INFO << "Opening maptex from idx=" << idxPath << " mul=" << path << std::endl;
    mapTexLoader_.reset(new MapTexLoader(idxPath, path));

    checkFileExists("artidx.mul");
    checkFileExists("art.mul");
    idxPath = filePathMap_["artidx.mul"];
    path = filePathMap_["art.mul"];
    LOG_INFO << "Opening art from idx=" << idxPath << " mul=" << path << std::endl;
    artLoader_.reset(new ArtLoader(idxPath, path));

    checkFileExists("gumpidx.mul");
    checkFileExists("gumpart.mul");
    idxPath = filePathMap_["gumpidx.mul"];
    path = filePathMap_["gumpart.mul"];
    LOG_INFO << "Opening gump art from idx=" << idxPath << " mul=" << path << std::endl;
    gumpArtLoader_.reset(new GumpArtLoader(idxPath, path));

    checkFileExists("animdata.mul");
    path = filePathMap_["animdata.mul"];
    LOG_INFO << "Opening animdata from mul=" << path << std::endl;
    animDataLoader_.reset(new AnimDataLoader(path));


    unsigned int blockCountX;
    unsigned int blockCountY;
    bool difsEnabled;


    // map0.mul is the only file that is absolutely required, all others are optional
    checkFileExists("map0.mul");

    std::stringstream ss;
    for (unsigned int index = 0; index <= 5; ++index) {
        ss.str(""); ss.clear();
        ss << "/fluo/files/map" << index << "@enabled";

        if (!config[ss.str().c_str()].asBool()) {
            continue;
        }

        ss.str(""); ss.clear();
        ss << "map" << index << ".mul";
        if (!hasPathFor(ss.str())) {
            // file does not exist
            if (index == 1) {
                // old clients use map0.mul also for map1
                ss.str(""); ss.clear();
                ss << "map0.mul";
                LOG_INFO << "Using map0.mul instead of map1.mul" << std::endl;
            } else {
                LOG_WARN << "Could not find map file " << ss.str() << ", although this map is enabled" << std::endl;
                continue;
            }
        }
        path = filePathMap_[ss.str()];

        ss.str(""); ss.clear();
        ss << "/fluo/files/map" << index << "@width";
        blockCountX = config[ss.str().c_str()].asInt();

        ss.str(""); ss.clear();
        ss << "/fluo/files/map" << index << "@height";
        blockCountY = config[ss.str().c_str()].asInt();

        ss.str(""); ss.clear();
        ss << "/fluo/files/map" << index << "@difs-enabled";
        difsEnabled = config[ss.str().c_str()].asBool();

        if (difsEnabled) {
            // difs are not really mandatory, so no hard checkFileExists here
            ss.str(""); ss.clear();
            ss << "mapdifl" << index << ".mul";
            difOffsetsPath = filePathMap_[ss.str()];

            ss.str(""); ss.clear();
            ss << "mapdif" << index << ".mul";
            difPath = filePathMap_[ss.str()];

            LOG_INFO << "Opening map" << index << " from mul=" << path << ", dif-offsets=" << difOffsetsPath <<
                    ", dif=" << difPath << ", blockCountX=" << blockCountX << ", blockCountY=" << blockCountY << std::endl;
            mapLoader_[index].reset(new MapLoader(path, difOffsetsPath, difPath, blockCountX, blockCountY));
        } else {
            LOG_INFO << "Opening map" << index << " from mul=" << path << ", difs disabled, blockCountX=" <<
                    blockCountX << ", blockCountY=" << blockCountY << std::endl;
            mapLoader_[index].reset(new MapLoader(path, blockCountX, blockCountY));
        }

        if (!fallbackMapLoader_.get()) {
            fallbackMapLoader_ = mapLoader_[index];
        }


        ss.str(""); ss.clear();
        ss << "staidx" << index << ".mul";
        if (!hasPathFor(ss.str())) {
            // file does not exist
            if (index == 1) {
                // old clients use staidx0.mul also for map 1
                ss.str(""); ss.clear();
                ss << "staidx0.mul";
                LOG_INFO << "Using staidx0.mul instead of staidx1.mul" << std::endl;
            } else {
                LOG_WARN << "Could not find staidx file " << ss.str() << ", although this map is enabled" << std::endl;
                continue;
            }
        }
        idxPath = filePathMap_[ss.str()];

        ss.str(""); ss.clear();
        ss << "statics" << index << ".mul";
        if (!hasPathFor(ss.str())) {
            // file does not exist
            if (index == 1) {
                // old clients use statics0.mul also for map 1
                ss.str(""); ss.clear();
                ss << "statics0.mul";
                LOG_INFO << "Using statics0.mul instead of statics1.mul" << std::endl;
            } else {
                LOG_WARN << "Could not find statics file " << ss.str() << ", although this map is enabled" << std::endl;
                continue;
            }
        }
        path = filePathMap_[ss.str()];



        if (difsEnabled) {
            ss.str(""); ss.clear();
            ss << "stadifl" << index << ".mul";
            //checkFileExists(ss.str());
            difOffsetsPath = filePathMap_[ss.str()];

            ss.str(""); ss.clear();
            ss << "stadifi" << index << ".mul";
            //checkFileExists(ss.str());
            difIdxPath = filePathMap_[ss.str()];

            ss.str(""); ss.clear();
            ss << "stadif" << index << ".mul";
            //checkFileExists(ss.str());
            difPath = filePathMap_[ss.str()];

            LOG_INFO << "Opening statics" << index << " from idx=" << idxPath << ", mul=" << path << ", dif-offsets=" << difOffsetsPath <<
                    ", dif-idx=" << difIdxPath << ", dif=" << difPath << std::endl;
            staticsLoader_[index].reset(new StaticsLoader(idxPath, path, difOffsetsPath, difIdxPath, difPath, blockCountX, blockCountY));
        } else {
            LOG_INFO << "Opening statics" << index << " from idx=" << idxPath << ", mul=" << path << ", difs disabled" << std::endl;
            staticsLoader_[index].reset(new StaticsLoader(idxPath, path, blockCountX, blockCountY));
        }

        if (!fallbackStaticsLoader_.get()) {
            fallbackStaticsLoader_ = staticsLoader_[index];
        }
    }



    unsigned int highDetailCount;
    unsigned int lowDetailCount;
    const char* animNames[] = { "anim", "anim2", "anim3", "anim4", "anim5" };
    for (unsigned int index = 0; index < 5; ++index) {
        ss.str(""); ss.clear();
        ss << "/fluo/files/" << animNames[index] << "@highdetail";
        highDetailCount = config[ss.str().c_str()].asInt();

        ss.str(""); ss.clear();
        ss << "/fluo/files/" << animNames[index] << "@lowdetail";
        lowDetailCount = config[ss.str().c_str()].asInt();

        ss.str(""); ss.clear();
        ss << animNames[index] << ".idx";
        if (!hasPathFor(ss.str())) {
            LOG_WARN << "Animations file " << ss.str() << " not found" << std::endl;
            continue;
        }
        idxPath = filePathMap_[ss.str()];

        ss.str(""); ss.clear();
        ss << animNames[index] << ".mul";
        if (!hasPathFor(ss.str())) {
            LOG_WARN << "Animations file " << ss.str() << " not found" << std::endl;
            continue;
        }
        path = filePathMap_[ss.str()];

        LOG_INFO << "Opening " << animNames[index] << " from idx=" << idxPath << ", mul=" << path << ", high-detail=" <<
                highDetailCount << ", low-detail=" << lowDetailCount << std::endl;
        animLoader_[index].reset(new AnimLoader(idxPath, path, highDetailCount, lowDetailCount));

        if (!fallbackAnimLoader_.get()) {
            fallbackAnimLoader_ = animLoader_[index];
        }
    }

    // check if at least one anim mul file was found
    if (!fallbackAnimLoader_) {
        LOG_EMERGENCY << "No anim*.mul file found, exiting" << std::endl;
        return false;
    }


    if (hasPathFor("mobtypes.txt")) {
        path = filePathMap_["mobtypes.txt"];
        LOG_INFO << "Opening mobtypes.txt from path=" << path << std::endl;
        mobTypesLoader_.reset(new DefFileLoader<MobTypeDef>(path, "isi", &MobTypesLoader::parseType));
    } else {
        LOG_WARN << "mobtypes.txt not found" << std::endl;
        mobTypesLoader_.reset(new DefFileLoader<MobTypeDef>());
    }


    const char* unifontNames[] = { "unifont", "unifont1", "unifont2", "unifont3", "unifont4", "unifont5", "unifont6", "unifont7", "unifont8", "unifont9", "unifont10", "unifont11", "unifont12" };

    for (unsigned int index = 0; index <= 12; ++index) {
        ss.str(""); ss.clear();
        ss << unifontNames[index] << ".mul";
        if (hasPathFor(ss.str())) {
            path = filePathMap_[ss.str()];
            LOG_INFO << "Opening " << unifontNames[index] << ".mul from path=" << path << std::endl;
            uniFontLoader_[index].reset(new UniFontLoader(path));
            if (!fallbackUniFontLoader_) {
                fallbackUniFontLoader_ = uniFontLoader_[index];
            }
        } else {
            LOG_WARN << "Unable to find " << unifontNames[index] << ".mul" << std::endl;
        }
    }

    checkFileExists("soundidx.mul");
    checkFileExists("sound.mul");
    idxPath = filePathMap_["soundidx.mul"];
    path = filePathMap_["sound.mul"];
    LOG_INFO << "Opening sound from idx=" << idxPath << " mul=" << path << std::endl;
    soundLoader_.reset(new SoundLoader(idxPath, path));

    checkFileExists("skills.idx");
    checkFileExists("skills.mul");
    idxPath = filePathMap_["skills.idx"];
    path = filePathMap_["skills.mul"];
    LOG_INFO << "Opening skills from idx=" << idxPath << " mul=" << path << std::endl;
    skillsLoader_.reset(new SkillsLoader(idxPath, path));

    checkFileExists("radarcol.mul");
    path = filePathMap_["radarcol.mul"];
    LOG_INFO << "Opening radarcol from mul=" << path << std::endl;
    radarColLoader_.reset(new RadarColLoader(path));


    checkFileExists("body.def");
    path = filePathMap_["body.def"];
    LOG_INFO << "Opening body.def from path=" << path << std::endl;
    bodyDefLoader_.reset(new DefFileLoader<BodyDef>(path, "iri"));

    checkFileExists("bodyconv.def");
    path = filePathMap_["bodyconv.def"];
    LOG_INFO << "Opening bodyconv.def from path=" << path << std::endl;
    bodyConvDefLoader_.reset(new DefFileLoader<BodyConvDef>(path, "iiiii"));

    checkFileExists("paperdoll.def");
    path = filePathMap_["paperdoll.def"];
    LOG_INFO << "Opening paperdoll.def from path=" << path << std::endl;
    paperdollDefLoader_.reset(new DefFileLoader<PaperdollDef>(path, "iiii"));

    checkFileExists("gump.def");
    path = filePathMap_["gump.def"];
    LOG_INFO << "Opening gump.def from path=" << path << std::endl;
    gumpDefLoader_.reset(new DefFileLoader<GumpDef>(path, "iri"));

    if (hasPathFor("equipconv.def")) {
        path = filePathMap_["equipconv.def"];
        LOG_INFO << "Opening equipconv.def from path=" << path << std::endl;
        equipConvDefLoader_.reset(new EquipConvDefLoader(path));
    } else {
        LOG_WARN << "equipconv.def not found" << std::endl;
        equipConvDefLoader_.reset(new EquipConvDefLoader());
    }

    checkFileExists("mount.def");
    path = filePathMap_["mount.def"];
    LOG_INFO << "Opening mount.def from path=" << path << std::endl;
    mountDefLoader_.reset(new DefFileLoader<MountDef>(path, "ii"));

    checkFileExists("effecttranslation.def");
    path = filePathMap_["effecttranslation.def"];
    LOG_INFO << "Opening effecttranslation.def from path=" << path << std::endl;
    effectTranslationDefLoader_.reset(new DefFileLoader<EffectTranslationDef>(path, "is", &EffectTranslationDef::setEffectName));

    checkFileExists("music/digital/config.txt");
    path = filePathMap_["music/digital/config.txt"];
    LOG_INFO << "Opening sound config.txt from path=" << path << std::endl;
    musicConfigDefLoader_.reset(new DefFileLoader<MusicConfigDef>(path, "is", &MusicConfigDef::parse));

    checkFileExists("sound.def");
    path = filePathMap_["sound.def"];
    LOG_INFO << "Opening sound.def from path=" << path << std::endl;
    soundDefLoader_.reset(new DefFileLoader<SoundDef>(path, "iri"));

    checkFileExists("spellbooks.xml");
    path = filePathMap_["spellbooks.xml"];
    LOG_INFO << "Opening spellbooks.xml from path=" << path << std::endl;
    spellbooks_.reset(new Spellbooks(path));

    checkFileExists("cliloc.enu");
    path = filePathMap_["cliloc.enu"];
    LOG_INFO << "Opening cliloc.enu from path=" << path << std::endl;
    clilocLoader_.reset(new ClilocLoader());
    clilocLoader_->indexFile(path, true);

    ss.str(""); ss.clear();
    UnicodeString langString = config["/fluo/files/cliloc@language"].asString();
    langString.toLower();
    ss << "cliloc." << StringConverter::toUtf8String(langString);
    if (ss.str() != "cliloc.enu") {
        checkFileExists(ss.str());
        path = filePathMap_[ss.str()];
        LOG_INFO << "Opening " << ss.str() << " from path=" << path << std::endl;
        clilocLoader_->indexFile(path, false);
    }


    initOverrides();

    return true;
}

Manager::~Manager() {
    LOG_INFO << "data::Manager shutdown" << std::endl;
}

boost::shared_ptr<MapLoader> Manager::getMapLoader(unsigned int index) {
    if (index > 4) {
        LOG_WARN << "Trying to access map loader for map index " << index << std::endl;
        index = 0;
    }

    Manager* sing = getSingleton();

    boost::shared_ptr<MapLoader> ret = sing->mapLoader_[index];
    if (ret) {
        return ret;
    } else {
        LOG_WARN << "Trying to access uninitialized map loader for map index " << index << std::endl;
        return sing->fallbackMapLoader_;
    }
}

boost::shared_ptr<StaticsLoader> Manager::getStaticsLoader(unsigned int index) {
    if (index > 4) {
        LOG_WARN << "Trying to access statics loader for map index " << index << std::endl;
        index = 0;
    }

    Manager* sing = getSingleton();

    boost::shared_ptr<StaticsLoader> ret = sing->staticsLoader_[index];
    if (ret) {
        return ret;
    } else {
        LOG_WARN << "Trying to access uninitialized statics loader for map index " << index << std::endl;
        return sing->fallbackStaticsLoader_;
    }
}

boost::shared_ptr<ui::TextureProvider> Manager::getItemTextureProvider(unsigned int artId) {
    boost::shared_ptr<ui::TextureProvider> ret;

    // check for animate flag in tiledata
    if (getTileDataLoader()->getStaticTileInfo(artId)->animation()) {
        // if exists, load complex textureprovider
        ret.reset(new ui::AnimDataTextureProvider(artId));
    } else {
        // if not, just load the simple one
        ret.reset(new ui::SingleTextureProvider(TextureSource::STATICART, artId));
    }

    return ret;
}

unsigned int Manager::getAnimType(unsigned int bodyId) {
    Manager* sing = getSingleton();

    BodyConvDef bodyConvEntry = sing->bodyConvDefLoader_->get(bodyId);
    boost::shared_ptr<AnimLoader> ldr;
    if (bodyConvEntry.bodyId_ != 0) {
        //LOG_DEBUG << "redirecting anim! body=" << bodyId << " fileIdx=" << bodyConvEntry.getAnimFileIdx() << " idx in file=" << bodyConvEntry.getAnimIdxInFile() << std::endl;
        ldr = getAnimLoader(bodyConvEntry.getAnimFileIdx());
    } else {
        ldr = getAnimLoader(0);
    }

    return ldr->getAnimType(bodyId);
}

std::vector<boost::shared_ptr<ui::Animation> > Manager::getAnim(unsigned int bodyId, unsigned int animId) {
    Manager* sing = getSingleton();

    BodyConvDef bodyConvEntry = sing->bodyConvDefLoader_->get(bodyId);
    boost::shared_ptr<AnimLoader> ldr;
    unsigned int animIdx;
    if (bodyConvEntry.bodyId_ != 0) {
        //LOG_DEBUG << "redirecting anim! body=" << bodyId << " fileIdx=" << bodyConvEntry.getAnimFileIdx() << " idx in file=" << bodyConvEntry.getAnimIdxInFile() << std::endl;
        animIdx = bodyConvEntry.getAnimIdxInFile();
        ldr = getAnimLoader(bodyConvEntry.getAnimFileIdx());
    } else {
        ldr = getAnimLoader(0);
        animIdx = bodyId;
    }

    std::vector<boost::shared_ptr<ui::Animation> > ret;

    boost::shared_ptr<ui::Animation> tmpDown = ldr->getAnimation(animIdx, animId, 0);
    boost::shared_ptr<ui::Animation> tmpDownLeft = ldr->getAnimation(animIdx, animId, 1);
    boost::shared_ptr<ui::Animation> tmpLeft = ldr->getAnimation(animIdx, animId, 2);
    boost::shared_ptr<ui::Animation> tmpUpLeft = ldr->getAnimation(animIdx, animId, 3);
    boost::shared_ptr<ui::Animation> tmpUp = ldr->getAnimation(animIdx, animId, 4);

    // mirrored
    ret.push_back(tmpUpLeft);
    ret.push_back(tmpLeft);
    ret.push_back(tmpDownLeft);

    ret.push_back(tmpDown);
    ret.push_back(tmpDownLeft);
    ret.push_back(tmpLeft);
    ret.push_back(tmpUpLeft);
    ret.push_back(tmpUp);

    return ret;
}

boost::shared_ptr<AnimLoader> Manager::getAnimLoader(unsigned int index) {
    if (index > 5) {
        LOG_WARN << "Trying to access anim loader with index " << index << std::endl;
        index = 0;
    }

    Manager* sing = getSingleton();

    boost::shared_ptr<AnimLoader> ret = sing->animLoader_[index];
    if (ret) {
        return ret;
    } else {
        LOG_WARN << "Trying to access uninitialized anim loader with index " << index << std::endl;
        return sing->fallbackAnimLoader_;
    }
}

boost::shared_ptr<UniFontLoader> Manager::getUniFontLoader(unsigned int index) {
    if (index > 12) {
        LOG_WARN << "Trying to access unifont loader with index " << index << std::endl;
        index = 0;
    }

    Manager* sing = getSingleton();

    boost::shared_ptr<UniFontLoader> ret = sing->uniFontLoader_[index];
    if (ret) {
        return ret;
    } else {
        LOG_WARN << "Trying to access uninitialized unifont loader with index " << index << std::endl;
        return sing->fallbackUniFontLoader_;
    }
}

BodyDef Manager::getBodyDef(unsigned int baseBodyId) {
    Manager* sing = getSingleton();

    // a little strange, but bodyconv.def overwrites body.def. So if there is a bodyconv entry, return foo
    if (sing->bodyConvDefLoader_->hasValue(baseBodyId)) {
        return BodyDef();
    }

    return sing->bodyDefLoader_->get(baseBodyId);
}

PaperdollDef Manager::getPaperdollDef(unsigned int bodyId) {
    Manager* sing = getSingleton();
    return sing->paperdollDefLoader_->get(bodyId);
}

GumpDef Manager::getGumpDef(unsigned int gumpId) {
    Manager* sing = getSingleton();
    return sing->gumpDefLoader_->get(gumpId);
}

MountDef Manager::getMountDef(unsigned int itemId) {
    Manager* sing = getSingleton();
    return sing->mountDefLoader_->get(itemId);
}

MobTypeDef Manager::getMobTypeDef(unsigned int bodyId) {
    Manager* sing = getSingleton();
    return sing->mobTypesLoader_->get(bodyId);
}

EffectTranslationDef Manager::getEffectTranslationDef(unsigned int effectId) {
    Manager* sing = getSingleton();
    return sing->effectTranslationDefLoader_->get(effectId);
}

MusicConfigDef Manager::getMusicConfigDef(unsigned int musicId) {
    Manager* sing = getSingleton();
    return sing->musicConfigDefLoader_->get(musicId);
}

SoundDef Manager::getSoundDef(unsigned int soundId) {
    Manager* sing = getSingleton();
    return sing->soundDefLoader_->get(soundId);
}

const boost::filesystem::path& Manager::getFilePathFor(const UnicodeString& string) {
    return getSingleton()->filePathMap_[StringConverter::toUtf8String(string)];
}

void Manager::buildFilePathMap(Config& config) {
    boost::filesystem::path mulDirPath = config["/fluo/files/mul-directory@path"].asPath();
    if (!boost::filesystem::exists(mulDirPath) || !boost::filesystem::is_directory(mulDirPath)) {
        LOG_EMERGENCY << "Invalid mul directory" << std::endl;
        throw Exception("Invalid mul directory");
    }

    addToFilePathMap(mulDirPath, true);

    boost::filesystem::path fluoDataPath("data");
    addToFilePathMap(fluoDataPath, true);

    boost::filesystem::path shardDataPath = config.getShardPath() / "data";
    addToFilePathMap(shardDataPath, true);
}

void Manager::addToFilePathMap(const boost::filesystem::path& directory, bool addSubdirectories, const UnicodeString& prefix) {
    namespace bfs = boost::filesystem;

    if (!bfs::exists(directory) || !bfs::is_directory(directory)) {
        return;
    }

    bfs::directory_iterator nameIter(directory);
    bfs::directory_iterator nameEnd;

    for (; nameIter != nameEnd; ++nameIter) {
        if (bfs::is_directory(nameIter->status())) {
            if (addSubdirectories) {
                UnicodeString nextPre = prefix + StringConverter::fromUtf8(nameIter->path().filename().string()) + "/";
                addToFilePathMap(nameIter->path(), true, nextPre);
            }
        }

        UnicodeString str = prefix + StringConverter::fromUtf8(nameIter->path().filename().string());
        str.toLower();

        filePathMap_[StringConverter::toUtf8String(str)] = nameIter->path();
    }
}

bool Manager::hasPathFor(const std::string& file) {
    Manager* sing = getSingleton();
    return sing->filePathMap_.find(file) != sing->filePathMap_.end();
}

void Manager::checkFileExists(const std::string& file) const {
    if (!hasPathFor(file)) {
        LOG_ERROR << "Required file " << file << " was not found" << std::endl;
        throw std::exception();
    }
}

unsigned int Manager::getGumpIdForItem(unsigned int itemId, unsigned int parentBodyId) {
    Manager* sing = getSingleton();

    unsigned int animId = sing->tileDataLoader_->getStaticTileInfo(itemId)->animId_;

    EquipConvDef equipConv = sing->equipConvDefLoader_->get(parentBodyId, itemId);
    if (equipConv.bodyId_ != 0) {
        return equipConv.getGumpId();
    }

    PaperdollDef pdDef = sing->paperdollDefLoader_->get(parentBodyId);
    if (pdDef.bodyId_ == 0) {
        LOG_ERROR << "Unable to translate gump ids for body " << parentBodyId << std::endl;
        return 1;
    }

    unsigned int gumpId = animId + pdDef.gumpOffset_;
    GumpDef gumpDef = sing->getGumpDef(gumpId);
    if (gumpDef.gumpId_ != 0) {
        gumpId = gumpDef.translateId_;
    }

    if (!sing->gumpArtLoader_->hasTexture(gumpId) && sing->gumpArtOverrides_.find(gumpId) == sing->gumpArtOverrides_.end()) {
        gumpId = animId + pdDef.gumpOffsetFallback_;
        data::GumpDef gumpDef2 = sing->getGumpDef(gumpId);
        if (gumpDef2.gumpId_ != 0) {
            gumpId = gumpDef2.translateId_;
        }
    }

    return gumpId;
}

boost::shared_ptr<ClilocLoader> Manager::getClilocLoader() {
    return getSingleton()->clilocLoader_;
}

boost::shared_ptr<TileDataLoader> Manager::getTileDataLoader() {
    return getSingleton()->tileDataLoader_;
}

boost::shared_ptr<HuesLoader> Manager::getHuesLoader() {
    return getSingleton()->huesLoader_;
}

boost::shared_ptr<AnimDataLoader> Manager::getAnimDataLoader() {
    return getSingleton()->animDataLoader_;
}

boost::shared_ptr<SoundLoader> Manager::getSoundLoader() {
    return getSingleton()->soundLoader_;
}

boost::shared_ptr<SkillsLoader> Manager::getSkillsLoader() {
    return getSingleton()->skillsLoader_;
}

boost::shared_ptr<RadarColLoader> Manager::getRadarColLoader() {
    return getSingleton()->radarColLoader_;
}


boost::filesystem::path Manager::getShardFilePath(const boost::filesystem::path& innerPath) {
    boost::filesystem::path ret;

    if (Client::getSingleton()->getConfig().isLoaded()) {
        ret = Client::getSingleton()->getConfig().getShardPath() / innerPath;
    }

    if (!boost::filesystem::exists(ret)) {
        ret = innerPath;

        if (!boost::filesystem::exists(ret)) {
            LOG_ERROR << "File not found: " << innerPath << std::endl;
            return boost::filesystem::path();
        }
    }

    return ret;
}

boost::shared_ptr<ui::Texture> Manager::getTexture(unsigned int source, unsigned int id) {
    Manager* sing = getSingleton();
    boost::shared_ptr<ui::Texture> ret;
    std::map<unsigned int, boost::filesystem::path>::const_iterator iter;

    switch (source) {
    case TextureSource::HTTP:
    case TextureSource::FILE:
    case TextureSource::THEME:
        LOG_ERROR << "Unable to handle getTexture(int, int) for file, http or theme source" << std::endl;
        break;

    case TextureSource::MAPART:
        iter = sing->mapArtOverrides_.find(id);
        if (iter != sing->mapArtOverrides_.end()) {
            ret = sing->filePathLoader_->getTexture(iter->second);
            ret->setUsage(ui::Texture::USAGE_WORLD);
        } else {
            ret = sing->artLoader_->getMapTexture(id);
        }
        break;

    case TextureSource::STATICART:
        iter = sing->staticArtOverrides_.find(id);
        if (iter != sing->staticArtOverrides_.end()) {
            ret = sing->filePathLoader_->getTexture(iter->second);
            ret->setUsage(ui::Texture::USAGE_WORLD);
        } else {
            ret = sing->artLoader_->getItemTexture(id);
        }
        break;

    case TextureSource::MAPTEX:
        iter = sing->mapTexOverrides_.find(id);
        if (iter != sing->mapTexOverrides_.end()) {
            ret = sing->filePathLoader_->getTexture(iter->second);
            ret->setUsage(ui::Texture::USAGE_WORLD);
        } else {
            ret = sing->mapTexLoader_->get(id);
        }
        break;

    case TextureSource::GUMPART:
        iter = sing->gumpArtOverrides_.find(id);
        if (iter != sing->gumpArtOverrides_.end()) {
            ret = sing->filePathLoader_->getTexture(iter->second);
            ret->setUsage(ui::Texture::USAGE_GUMP);
        } else {
            ret = sing->gumpArtLoader_->getTexture(id);
        }
        break;

    default:
        LOG_ERROR << "Unknown texture source \"" << source << "\"" << std::endl;
        break;
    }

    return ret;
}

boost::shared_ptr<ui::Texture> Manager::getTexture(unsigned int source, const UnicodeString& id) {
    boost::shared_ptr<ui::Texture> ret;
    switch (source) {
    case TextureSource::FILE: {
        boost::filesystem::path idPath(StringConverter::toUtf8String(id));
        idPath = getShardFilePath(idPath);
        ret = getSingleton()->filePathLoader_->getTexture(idPath);
        ret->setUsage(ui::Texture::USAGE_GUMP);
        break;
    }

    case TextureSource::HTTP:
        ret = getSingleton()->httpLoader_->getTexture(id);
        ret->setUsage(ui::Texture::USAGE_GUMP);
        break;

    case TextureSource::THEME: {
        boost::filesystem::path idPath = ui::Manager::getSingleton()->getThemePath() / StringConverter::toUtf8String(id);
        ret = getSingleton()->filePathLoader_->getTexture(idPath);
        ret->setUsage(ui::Texture::USAGE_GUMP);
        break;
    }

    case TextureSource::MAPTEX:
    case TextureSource::MAPART:
    case TextureSource::STATICART:
    case TextureSource::GUMPART: {
        int idInt = StringConverter::toInt(id);
        ret = getTexture(source, idInt);
        break;
    }

    default:
        LOG_ERROR << "Unknown texture source \"" << source << "\"" << std::endl;
    }

    return ret;
}

boost::shared_ptr<ui::Texture> Manager::getTexture(const UnicodeString& source, const UnicodeString& id) {
    UnicodeString lowerSource(source);
    lowerSource.toLower();
    unsigned int sourceId = 0;
    if (lowerSource == "file") {
        sourceId = TextureSource::FILE;
    } else if (lowerSource == "mapart") {
        sourceId = TextureSource::MAPART;
    } else if (lowerSource == "staticart") {
        sourceId = TextureSource::STATICART;
    } else if (lowerSource == "gumpart") {
        sourceId = TextureSource::GUMPART;
    } else if (lowerSource == "http") {
        sourceId = TextureSource::HTTP;
    } else if (lowerSource == "theme") {
        sourceId = TextureSource::THEME;
    } else if (lowerSource == "maptex") {
        sourceId = TextureSource::MAPTEX;
    }

    if (sourceId != 0) {
        return getTexture(sourceId, id);
    } else {
        LOG_ERROR << "Unknown texture source \"" << source << "\"" << std::endl;
        boost::shared_ptr<ui::Texture> ret;
        return ret;
    }
}

const SpellbookInfo* Manager::getSpellbookInfo(unsigned int packetOffset) {
    return getSingleton()->spellbooks_->get(packetOffset);
}

void Manager::initOverrides() {
    namespace bfs = boost::filesystem;

    bfs::path globalStaticDir("data/overrides/staticart");
    if (bfs::exists(globalStaticDir) && bfs::is_directory(globalStaticDir)) {
        addOverrideDirectory(staticArtOverrides_, globalStaticDir);
    }

    if (hasPathFor("overrides/staticart")) {
        bfs::path shardStaticDir = filePathMap_["overrides/staticart"];
        if (bfs::is_directory(shardStaticDir) && shardStaticDir != globalStaticDir) {
            addOverrideDirectory(staticArtOverrides_, shardStaticDir);
        }
    }

    bfs::path globalMapDir("data/overrides/mapart");
    if (bfs::exists(globalMapDir) && bfs::is_directory(globalMapDir)) {
        addOverrideDirectory(mapArtOverrides_, globalMapDir);
    }

    if (hasPathFor("overrides/mapart")) {
        bfs::path shardMapDir = filePathMap_["overrides/mapart"];
        if (bfs::is_directory(shardMapDir) && shardMapDir != globalMapDir) {
            addOverrideDirectory(mapArtOverrides_, shardMapDir);
        }
    }

    bfs::path globalGumpDir("data/overrides/gumpart");
    if (bfs::exists(globalGumpDir) && bfs::is_directory(globalGumpDir)) {
        addOverrideDirectory(gumpArtOverrides_, globalGumpDir);
    }

    if (hasPathFor("overrides/gumpart")) {
        bfs::path shardGumpDir = filePathMap_["overrides/gumpart"];
        if (bfs::is_directory(shardGumpDir) && shardGumpDir != globalGumpDir) {
            addOverrideDirectory(gumpArtOverrides_, shardGumpDir);
        }
    }

    bfs::path globalTexDir("data/overrides/maptex");
    if (bfs::exists(globalTexDir) && bfs::is_directory(globalTexDir)) {
        addOverrideDirectory(mapTexOverrides_, globalTexDir);
    }

    if (hasPathFor("overrides/maptex")) {
        bfs::path shardTexDir = filePathMap_["overrides/maptex"];
        if (bfs::is_directory(shardTexDir) && shardTexDir != globalTexDir) {
            addOverrideDirectory(mapTexOverrides_, shardTexDir);
        }
    }
}

void Manager::addOverrideDirectory(std::map<unsigned int, boost::filesystem::path>& map, boost::filesystem::path directory) {
    LOG_DEBUG << "Scanning override directory: " << directory.string() << std::endl;
    namespace bfs = boost::filesystem;
    bfs::directory_iterator nameIter(directory);
    bfs::directory_iterator nameEnd;

    for (; nameIter != nameEnd; ++nameIter) {
        if (bfs::is_regular_file(nameIter->status()) && nameIter->path().filename() != ".svn") {
            int parsedNum = StringConverter::toInt(nameIter->path().stem().c_str());
            if (parsedNum > 0) {
                map[parsedNum] = nameIter->path();
            } else {
                LOG_WARN << "Invalid file in override directory: " << nameIter->path() << std::endl;
            }
        }
    }
}

void Manager::reloadOverrides() {
    Manager* sing = getSingleton();
    sing->filePathLoader_->clearCache();
    sing->staticArtOverrides_.clear();
    sing->mapArtOverrides_.clear();
    sing->mapTexOverrides_.clear();
    sing->gumpArtOverrides_.clear();
    sing->initOverrides();
}

UnicodeString Manager::getItemName(unsigned int artId) {
    Manager* sing = getSingleton();
    unsigned int clilocId = (artId < 0x4000) ? (1020000 + artId) : (1078872 + artId);
    if (sing->clilocLoader_->hasEntry(clilocId)) {
        return sing->clilocLoader_->get(clilocId);
    } else {
        return sing->tileDataLoader_->getStaticTileInfo(artId)->name_;
    }
}

}
}
