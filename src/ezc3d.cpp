#define EZC3D_API_EXPORTS
#include "ezc3d.h"

ezc3d::c3d::c3d():
    _filePath(""),
    m_nByteToRead_float(4*ezc3d::DATA_TYPE::BYTE)
{
    c_float = new char[m_nByteToRead_float + 1];

    _header = std::shared_ptr<ezc3d::Header>(new ezc3d::Header());
    _parameters = std::shared_ptr<ezc3d::ParametersNS::Parameters>(new ezc3d::ParametersNS::Parameters());
    _data = std::shared_ptr<ezc3d::DataNS::Data>(new ezc3d::DataNS::Data());
}

ezc3d::c3d::c3d(const std::string &filePath):
    std::fstream(filePath, std::ios::in | std::ios::binary),
    _filePath(filePath),
    m_nByteToRead_float(4*ezc3d::DATA_TYPE::BYTE)
{
    c_float = new char[m_nByteToRead_float + 1];

    if (!is_open())
        throw std::ios_base::failure("Could not open the c3d file");

    // Read all the section
    _header = std::shared_ptr<ezc3d::Header>(new ezc3d::Header(*this));
    _parameters = std::shared_ptr<ezc3d::ParametersNS::Parameters>(new ezc3d::ParametersNS::Parameters(*this));
    // header may be inconsistent with the parameters, so it must be update to make sure sizes are consistent
    updateHeader();

    // Now read the actual data
    _data = std::shared_ptr<ezc3d::DataNS::Data>(new ezc3d::DataNS::Data(*this));
}

ezc3d::c3d::~c3d()
{
    delete c_float;
    close();
}

void ezc3d::c3d::updateHeader()
{
    // Parameter is always consider as the right value. If there is a discrepancy between them, change the header
    if (parameters().group("POINT").parameter("FRAMES").valuesAsInt()[0] != header().nbFrames()){
        _header->firstFrame(0);
        _header->lastFrame(parameters().group("POINT").parameter("FRAMES").valuesAsInt()[0]);
    }
    float pointRate(parameters().group("POINT").parameter("RATE").valuesAsFloat()[0]);
    float buffer(10000);
    if (static_cast<int>(pointRate*buffer) != static_cast<int>(header().frameRate()*buffer)){
        _header->frameRate(pointRate);
    }
    if (parameters().group("POINT").parameter("USED").valuesAsInt()[0] != header().nb3dPoints()){
        _header->nb3dPoints(parameters().group("POINT").parameter("USED").valuesAsInt()[0]);
    }

    float analogRate(parameters().group("ANALOG").parameter("RATE").valuesAsFloat()[0]);
    if (static_cast<int>(pointRate) != 0 && static_cast<int>((analogRate / pointRate) * buffer) != static_cast<int>(header().nbAnalogByFrame()*buffer)){
        _header->nbAnalogByFrame(static_cast<int>(analogRate / pointRate));
    }
    if (parameters().group("ANALOG").parameter("USED").valuesAsInt()[0] != header().nbAnalogs()){
        _header->nbAnalogs(parameters().group("ANALOG").parameter("USED").valuesAsInt()[0]);
    }
}

void ezc3d::c3d::updateParameters(const std::vector<std::string> &newMarkers, const std::vector<std::string> &newAnalogs)
{
    if (data().frames().size() != 0 && newMarkers.size() > 0)
        throw std::runtime_error("newMarkers in updateParameters should only be called on empty c3d");
    if (data().frames().size() != 0 && newAnalogs.size() > 0)
        throw std::runtime_error("newAnalogs in updateParameters should only be called on empty c3d");

    // If frames has been added
    ezc3d::ParametersNS::GroupNS::Group& grpPoint(_parameters->group_nonConst(parameters().groupIdx("POINT")));
    int nFrames(static_cast<int>(data().frames().size()));
    if (nFrames != grpPoint.parameter("FRAMES").valuesAsInt()[0]){
        int idx(grpPoint.parameterIdx("FRAMES"));
        grpPoint.parameters_nonConst()[static_cast<size_t>(idx)].set(std::vector<int>() = {nFrames}, {1});
    }

    // If points has been added
    int nPoints;
    if (data().frames().size() > 0)
        nPoints = static_cast<int>(data().frame(0).points().points().size());
    else
        nPoints = static_cast<int>(parameters().group("POINT").parameter("LABELS").valuesAsString().size() + newMarkers.size());
    if (nPoints != grpPoint.parameter("USED").valuesAsInt()[0]){
        grpPoint.parameters_nonConst()[static_cast<size_t>(grpPoint.parameterIdx("USED"))].set(std::vector<int>() = {nPoints}, {1});

        size_t idxLabels(static_cast<size_t>(grpPoint.parameterIdx("LABELS")));
        size_t idxDescriptions(static_cast<size_t>(grpPoint.parameterIdx("DESCRIPTIONS")));
        std::vector<std::string> labels;
        std::vector<std::string> descriptions;
        int longestName(-1);
        for (int i = 0; i<nPoints; ++i){
            std::string name;
            if (data().frames().size() == 0){
                if (i < static_cast<int>(parameters().group("POINT").parameter("LABELS").valuesAsString().size()))
                    name = parameters().group("POINT").parameter("LABELS").valuesAsString()[static_cast<size_t>(i)];
                else
                    name = newMarkers[static_cast<size_t>(i) - parameters().group("POINT").parameter("LABELS").valuesAsString().size()];
            } else {
                name = data().frame(0).points().point(i).name();
            }
            if (int(name.size()) > longestName)
                longestName = static_cast<int>(name.size());
            labels.push_back(name);
            descriptions.push_back("");
        }
        grpPoint.parameters_nonConst()[idxLabels].set(labels, {longestName, nPoints});
        grpPoint.parameters_nonConst()[idxDescriptions].set(descriptions, {0, nPoints});
    }

    // If analogous data has been added
    ezc3d::ParametersNS::GroupNS::Group& grpAnalog(_parameters->group_nonConst(parameters().groupIdx("ANALOG")));
    int nAnalogs;
    if (data().frames().size() > 0){
        if (data().frame(0).analogs().subframes().size() > 0)
            nAnalogs = static_cast<int>(data().frame(0).analogs().subframe(0).channels().size());
        else
            nAnalogs = 0;
    } else
        nAnalogs = static_cast<int>(parameters().group("ANALOG").parameter("LABELS").valuesAsString().size() + newAnalogs.size());
    if (nAnalogs != grpAnalog.parameter("USED").valuesAsInt()[0]){
        grpAnalog.parameters_nonConst()[static_cast<size_t>(grpAnalog.parameterIdx("USED"))].set(std::vector<int>() = {nAnalogs}, {1});

        size_t idxLabels(static_cast<size_t>(grpAnalog.parameterIdx("LABELS")));
        size_t idxDescriptions(static_cast<size_t>(grpAnalog.parameterIdx("DESCRIPTIONS")));
        std::vector<std::string> labels;
        std::vector<std::string> descriptions;
        int longestName(-1);
        for (int i = 0; i<nAnalogs; ++i){
            std::string name;
            if (data().frames().size() == 0){
                if (i < static_cast<int>(parameters().group("ANALOG").parameter("LABELS").valuesAsString().size()))
                    name = parameters().group("ANALOG").parameter("LABELS").valuesAsString()[static_cast<size_t>(i)];
                else
                    name = newAnalogs[static_cast<size_t>(i)-parameters().group("ANALOG").parameter("LABELS").valuesAsString().size()];
            } else {
                name = data().frame(0).analogs().subframe(0).channel(i).name();
            }
            if (int(name.size()) > longestName)
                longestName = static_cast<int>(name.size());
            labels.push_back(name);
            descriptions.push_back("");
        }
        grpAnalog.parameters_nonConst()[idxLabels].set(labels, {longestName, nAnalogs});
        grpAnalog.parameters_nonConst()[idxDescriptions].set(descriptions, {0, nAnalogs});

        int idxScale(static_cast<int>(grpAnalog.parameterIdx("SCALE")));
        std::vector<float> scales(grpAnalog.parameter(idxScale).valuesAsFloat());
        for (int i = static_cast<int>(grpAnalog.parameter(idxScale).valuesAsFloat().size()); i<nAnalogs; ++i)
            scales.push_back(1);
        grpAnalog.parameters_nonConst()[static_cast<size_t>(idxScale)].set(scales, {int(scales.size())});

        int idxOffset(grpAnalog.parameterIdx("OFFSET"));
        std::vector<int> offset(grpAnalog.parameter(idxOffset).valuesAsInt());
        for (int i = static_cast<int>(grpAnalog.parameter(idxOffset).valuesAsInt().size()); i<nAnalogs; ++i)
            offset.push_back(0);
        grpAnalog.parameters_nonConst()[static_cast<size_t>(idxOffset)].set(offset, {int(offset.size())});

        int idxUnits(grpAnalog.parameterIdx("UNITS"));
        std::vector<std::string> units(grpAnalog.parameter(idxUnits).valuesAsString());
        int longestUnit(-1);
        for (int i = static_cast<int>(grpAnalog.parameter(idxUnits).valuesAsString().size()); i<nAnalogs; ++i){
            units.push_back("V");
        }
        for (unsigned int i=0; i<units.size(); ++i)
            if (int(units[i].size()) > longestUnit)
                longestUnit = static_cast<int>(units[i].size());
        grpAnalog.parameters_nonConst()[static_cast<size_t>(idxUnits)].set(units, {longestUnit, int(units.size())});

    }
    updateHeader();
}

void ezc3d::c3d::write(const std::string& filePath) const
{
    std::fstream f(filePath, std::ios::out | std::ios::binary);

    // Write the header
    this->header().write(f);

    // Write the parameters
    this->parameters().write(f);

    // Write the data
    this->data().write(f);

    f.close();
}

void ezc3d::removeSpacesOfAString(std::string& s){
    // Remove the spaces at the end of the strings
    for (int i = static_cast<int>(s.size()); i >= 0; --i)
        if (s[s.size()-1] == ' ')
            s.pop_back();
        else
            break;
}
std::string ezc3d::toUpper(const std::string &str){
    std::string new_str = str;
    std::transform(new_str.begin(), new_str.end(), new_str.begin(), ::toupper);
    return new_str;
}


unsigned int ezc3d::c3d::hex2uint(const char * val, unsigned int len){
    int ret(0);
    for (unsigned int i=0; i<len; i++)
        ret |= static_cast<int>(static_cast<unsigned char>(val[i])) * static_cast<int>(pow(0x100, i));
    return static_cast<unsigned int>(ret);
}

int ezc3d::c3d::hex2int(const char * val, unsigned int len){
    unsigned int tp(hex2uint(val, len));

    // convert to signed int
    // Find max int value
    unsigned int max(0);
    for (unsigned int i=0; i<len; ++i)
        max |= 0xFF * static_cast<unsigned int>(pow(0x100, i));

    // If the value is over uint_max / 2 then it is a negative number
    int out;
    if (tp > max / 2)
        out = static_cast<int>(tp - max - 1);
    else
        out = static_cast<int>(tp);

    return out;
}

int ezc3d::c3d::hex2long(const char * val, int len){
    long ret(0);
    for (int i=0; i<len; i++)
        ret |= static_cast<long>(static_cast<unsigned char>(val[i])) * static_cast<long>(pow(0x100, i));
    return static_cast<int>(ret);
}

void ezc3d::c3d::readFile(unsigned int nByteToRead, char * c, int nByteFromPrevious,
                     const  std::ios_base::seekdir &pos)
{
    if (pos != 1)
        this->seekg (nByteFromPrevious, pos); // Move to number analogs
    this->read (c, nByteToRead);
    c[nByteToRead] = '\0'; // Make sure last char is NULL
}
void ezc3d::c3d::readChar(unsigned int nByteToRead, char * c,int nByteFromPrevious,
                     const  std::ios_base::seekdir &pos)
{
    c = new char[nByteToRead + 1];
    readFile(nByteToRead, c, nByteFromPrevious, pos);
}

std::string ezc3d::c3d::readString(unsigned int nByteToRead, int nByteFromPrevious,
                              const std::ios_base::seekdir &pos)
{
    char* c = new char[nByteToRead + 1];
    readFile(nByteToRead, c, nByteFromPrevious, pos);
    std::string out(c);
    delete[] c;
    return out;
}

int ezc3d::c3d::readInt(unsigned int nByteToRead, int nByteFromPrevious,
            const std::ios_base::seekdir &pos)
{
    char* c = new char[nByteToRead + 1];
    readFile(nByteToRead, c, nByteFromPrevious, pos);

    // make sure it is an int and not an unsigned int
    int out(hex2int(c, nByteToRead));
    delete[] c;
    return out;
}

int ezc3d::c3d::readUint(unsigned int nByteToRead, int nByteFromPrevious,
            const std::ios_base::seekdir &pos)
{
    char* c = new char[nByteToRead + 1];
    readFile(nByteToRead, c, nByteFromPrevious, pos);

    // make sure it is an int and not an unsigned int
    int out(static_cast<int>(hex2uint(c, nByteToRead)));
    delete[] c;
    return out;
}

float ezc3d::c3d::readFloat(int nByteFromPrevious,
                const std::ios_base::seekdir &pos)
{
    readFile(m_nByteToRead_float, c_float, nByteFromPrevious, pos);
    float out (*reinterpret_cast<float*>(c_float));
    return out;
}

void ezc3d::c3d::readMatrix(unsigned int dataLenghtInBytes, const std::vector<int> &dimension,
                       std::vector<int> &param_data, size_t currentIdx)
{
    for (int i=0; i<dimension[currentIdx]; ++i)
        if (currentIdx == dimension.size()-1)
            param_data.push_back (readInt(dataLenghtInBytes*ezc3d::DATA_TYPE::BYTE));
        else
            readMatrix(dataLenghtInBytes, dimension, param_data, currentIdx + 1);
}

void ezc3d::c3d::readMatrix(const std::vector<int> &dimension,
                       std::vector<float> &param_data, size_t currentIdx)
{
    for (int i=0; i<dimension[currentIdx]; ++i)
        if (currentIdx == dimension.size()-1)
            param_data.push_back (readFloat());
        else
            readMatrix(dimension, param_data, currentIdx + 1);
}

void ezc3d::c3d::readMatrix(const std::vector<int> &dimension,
                       std::vector<std::string> &param_data_string)
{
    std::vector<std::string> param_data_string_tp;
    _readMatrix(dimension, param_data_string_tp);

    // Vicon c3d stores text length on first dimension, I am not sure if
    // this is a standard or a custom made stuff. I implemented it like that for now
    if (dimension.size() == 1){
        std::string tp;
        for (int j = 0; j < dimension[0]; ++j)
            tp += param_data_string_tp[static_cast<size_t>(j)];
        ezc3d::removeSpacesOfAString(tp);
        param_data_string.push_back(tp);
    }
    else
        _dispatchMatrix(dimension, param_data_string_tp, param_data_string);
}

void ezc3d::c3d::_readMatrix(const std::vector<int> &dimension,
                       std::vector<std::string> &param_data, size_t currentIdx)
{
    for (int i=0; i<dimension[currentIdx]; ++i)
        if (currentIdx == dimension.size()-1)
            param_data.push_back(readString(ezc3d::DATA_TYPE::BYTE));
        else
            _readMatrix(dimension, param_data, currentIdx + 1);
}

size_t ezc3d::c3d::_dispatchMatrix(const std::vector<int> &dimension,
                                 const std::vector<std::string> &param_data_in,
                                 std::vector<std::string> &param_data_out, size_t idxInParam,
                                 size_t currentIdx)
{
    for (int i=0; i<dimension[currentIdx]; ++i)
        if (currentIdx == dimension.size()-1){
            std::string tp;
            for (int j = 0; j < dimension[0]; ++j){
                tp += param_data_in[idxInParam];
                ++idxInParam;
            }
            ezc3d::removeSpacesOfAString(tp);
            param_data_out.push_back(tp);
        }
        else
            idxInParam = _dispatchMatrix(dimension, param_data_in, param_data_out, idxInParam, currentIdx + 1);
    return idxInParam;
}

const ezc3d::Header& ezc3d::c3d::header() const
{
    return *_header;
}

const ezc3d::ParametersNS::Parameters& ezc3d::c3d::parameters() const
{
    return *_parameters;
}

const ezc3d::DataNS::Data& ezc3d::c3d::data() const
{
    return *_data;
}

void ezc3d::c3d::addParameter(const std::string &groupName, const ezc3d::ParametersNS::GroupNS::Parameter &p)
{
    int idx(parameters().groupIdx(groupName));
    if (idx < 0){
        _parameters->addGroup(ezc3d::ParametersNS::GroupNS::Group(groupName));
        idx = static_cast<int>(parameters().groups().size()-1);
    }
    _parameters->group_nonConst(idx).addParameter(p);

    // Do a sanity check on the header if important stuff like number of frames or number of elements is changed
    updateHeader();
}

void ezc3d::c3d::addFrame(const ezc3d::DataNS::Frame &f, int j)
{
    // Make sure f.points().points() is the same as data.f[ANY].points()
    std::vector<std::string> labels(parameters().group("POINT").parameter("LABELS").valuesAsString());
    if (labels.size() != 0 && f.points().points().size() != labels.size())
        throw std::runtime_error("Points in frame and already existing must be the same");
    for (size_t i=0; i<labels.size(); ++i)
        if (f.points().pointIdx(labels[i]) < 0)
            throw std::runtime_error("All markers must appears in the frames and points");

    int nPoints(parameters().group("POINT").parameter("USED").valuesAsInt()[0]);
    if (nPoints != 0 && static_cast<int>(f.points().points().size()) != nPoints)
        throw std::runtime_error("Points must be consistent in terms of number of points");

    int nAnalogs(parameters().group("POINT").parameter("USED").valuesAsInt()[0]);
    int subSize(static_cast<int>(f.analogs().subframes().size()));
    if (subSize != 0){
        int nChannel(static_cast<int>(f.analogs().subframes()[0].channels().size()));
        int nAnalogByFrames(header().nbAnalogByFrame());
        if (!(nAnalogs==0 && nAnalogByFrames==0) && ((nAnalogs != 0 && subSize == 0) || (nChannel != nAnalogs && subSize != nAnalogByFrames )))
            throw std::runtime_error("Analogs must be consistent with data in terms of data frequency");
    }

    // Replace the jth frame
    _data->frame(f, j);
    updateParameters();
}

void ezc3d::c3d::addMarker(const std::vector<ezc3d::DataNS::Frame>& frames)
{
    if (frames.size() == 0 || frames.size() != data().frames().size())
        throw std::runtime_error("Frames must have the same number as the frame count");
    if (frames[0].points().points().size() == 0)
        throw std::runtime_error("Points cannot be empty");

    std::vector<std::string> labels(parameters().group("POINT").parameter("LABELS").valuesAsString());
    for (int idx = 0; idx<static_cast<int>(frames[0].points().points().size()); ++idx){
        const std::string &name(frames[0].points().point(idx).name());
        for (size_t i=0; i<labels.size(); ++i)
            if (!name.compare(labels[i]))
                throw std::runtime_error("Marker already exists");

        for (size_t f=0; f<data().frames().size(); ++f)
            _data->frames_nonConst()[f].points_nonConst().add(frames[f].points().point(idx));
    }
    updateParameters();
}
void ezc3d::c3d::addMarker(const std::string &name){
    if (data().frames().size() > 0){
        std::vector<ezc3d::DataNS::Frame> dummy_frames;
        ezc3d::DataNS::Points3dNS::Points dummy_pts;
        ezc3d::DataNS::Points3dNS::Point emptyPoint;
        emptyPoint.name(name);
        dummy_pts.add(emptyPoint);
        ezc3d::DataNS::Frame frame;
        frame.add(dummy_pts);
        for (size_t f=0; f<data().frames().size(); ++f)
            dummy_frames.push_back(frame);
        addMarker(dummy_frames);
    } else {
        updateParameters({name});
    }
}

void ezc3d::c3d::addAnalog(const std::vector<ezc3d::DataNS::Frame> &frames)
{
    if (frames.size() != data().frames().size())
        throw std::runtime_error("Frames must have the same number of frames");
    if (static_cast<int>(frames[0].analogs().subframes().size()) != header().nbAnalogByFrame())
        throw std::runtime_error("Subrames must have the same number of subframes");
    if (frames[0].analogs().subframe(0).channels().size() == 0)
        throw std::runtime_error("Channels cannot be empty");

    std::vector<std::string> labels(parameters().group("ANALOG").parameter("LABELS").valuesAsString());
    for (int idx = 0; idx<static_cast<int>(frames[0].analogs().subframe(0).channels().size()); ++idx){
        const std::string &name(frames[0].analogs().subframe(0).channel(idx).name());
        for (size_t i=0; i<labels.size(); ++i)
            if (!name.compare(labels[i]))
                throw std::runtime_error("Analog channel already exists");

        for (int f=0; f<static_cast<int>(data().frames().size()); ++f){
            for (int sf=0; sf<header().nbAnalogByFrame(); ++sf){
                _data->frames_nonConst()[static_cast<size_t>(f)].analogs_nonConst().subframes_nonConst()[static_cast<size_t>(sf)].addChannel(frames[static_cast<size_t>(f)].analogs().subframe(sf).channel(idx));
            }
        }
    }
    updateParameters();
}

void ezc3d::c3d::addAnalog(const std::string &name)
{
    if (data().frames().size() > 0){
        std::vector<ezc3d::DataNS::Frame> dummy_frames;
        ezc3d::DataNS::AnalogsNS::SubFrame dummy_subframes;
        ezc3d::DataNS::AnalogsNS::Channel emptyChannel;
        emptyChannel.name(name);
        emptyChannel.value(0);
        ezc3d::DataNS::Frame frame;
        dummy_subframes.channels_nonConst().push_back(emptyChannel);
        for (int sf=0; sf<header().nbAnalogByFrame(); ++sf)
            frame.analogs_nonConst().addSubframe(dummy_subframes);
        for (size_t f=0; f<data().frames().size(); ++f)
            dummy_frames.push_back(frame);
        addAnalog(dummy_frames);
    } else {
        updateParameters({}, {name});
    }
}

