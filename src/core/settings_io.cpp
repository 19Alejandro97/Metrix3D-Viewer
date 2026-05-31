#include "settings_io.hpp"

#define _CRT_SECURE_NO_WARNINGS
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <cstdio>
#include <iostream>

using namespace rapidjson;

void SettingsIO::Load(const std::string& filepath) {
    FILE* fp = fopen(filepath.c_str(), "rb");
    if (!fp) return;

    char readBuffer[65536];
    FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    Document d;
    d.ParseStream(is);
    fclose(fp);

    if (d.HasParseError() || !d.IsObject()) return;

    auto& s = AppSettings::Instance();

    if (d.HasMember("colorEntityA") && d["colorEntityA"].IsArray() && d["colorEntityA"].Size() == 4) {
        for(int i=0; i<4; ++i) s.colorEntityA[i] = d["colorEntityA"][i].GetFloat();
    }
    if (d.HasMember("colorEntityB") && d["colorEntityB"].IsArray() && d["colorEntityB"].Size() == 4) {
        for(int i=0; i<4; ++i) s.colorEntityB[i] = d["colorEntityB"][i].GetFloat();
    }
    if (d.HasMember("useGradient") && d["useGradient"].IsBool()) {
        s.useGradient = d["useGradient"].GetBool();
    }
    if (d.HasMember("viewportBackgroundTop") && d["viewportBackgroundTop"].IsArray() && d["viewportBackgroundTop"].Size() == 3) {
        for(int i=0; i<3; ++i) s.viewportBackgroundTop[i] = d["viewportBackgroundTop"][i].GetFloat();
    }
    if (d.HasMember("viewportBackgroundBottom") && d["viewportBackgroundBottom"].IsArray() && d["viewportBackgroundBottom"].Size() == 3) {
        for(int i=0; i<3; ++i) s.viewportBackgroundBottom[i] = d["viewportBackgroundBottom"][i].GetFloat();
    }
    if (d.HasMember("currentUnit") && d["currentUnit"].IsInt()) {
        s.currentUnit = (AppSettings::UnitSystem)d["currentUnit"].GetInt();
    }
    if (d.HasMember("preserveCoordinatesOnImport") && d["preserveCoordinatesOnImport"].IsBool()) {
        s.preserveCoordinatesOnImport = d["preserveCoordinatesOnImport"].GetBool();
    }
    if (d.HasMember("continuousMeasurement") && d["continuousMeasurement"].IsBool()) {
        s.continuousMeasurement = d["continuousMeasurement"].GetBool();
    }
}

void SettingsIO::Save(const std::string& filepath) {
    auto& s = AppSettings::Instance();
    Document d;
    d.SetObject();
    Document::AllocatorType& allocator = d.GetAllocator();

    auto addVec4 = [&](const char* name, const glm::vec4& v) {
        Value arr(kArrayType);
        for(int i=0; i<4; ++i) arr.PushBack(v[i], allocator);
        d.AddMember(StringRef(name), arr, allocator);
    };
    auto addVec3 = [&](const char* name, const glm::vec3& v) {
        Value arr(kArrayType);
        for(int i=0; i<3; ++i) arr.PushBack(v[i], allocator);
        d.AddMember(StringRef(name), arr, allocator);
    };

    addVec4("colorEntityA", s.colorEntityA);
    addVec4("colorEntityB", s.colorEntityB);
    d.AddMember("useGradient", s.useGradient, allocator);
    addVec3("viewportBackgroundTop", s.viewportBackgroundTop);
    addVec3("viewportBackgroundBottom", s.viewportBackgroundBottom);
    d.AddMember("currentUnit", (int)s.currentUnit, allocator);
    d.AddMember("preserveCoordinatesOnImport", s.preserveCoordinatesOnImport, allocator);
    d.AddMember("continuousMeasurement", s.continuousMeasurement, allocator);

    FILE* fp = fopen(filepath.c_str(), "wb");
    if (!fp) return;
    char writeBuffer[65536];
    FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));
    Writer<FileWriteStream> writer(os);
    d.Accept(writer);
    fclose(fp);
}
