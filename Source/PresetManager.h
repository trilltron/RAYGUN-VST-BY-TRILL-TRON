#pragma once
#include <juce_core/juce_core.h>
#include "ParamTable.h"

namespace rg {
struct PresetInfo {
    juce::String name, author, type, character, desc, key;
    juce::StringArray tags, style, genre;
    int bpm = 0; bool factory = true; juce::File file;
    juce::var data;   // full JSON object ("a", "b", "global", "macros")
    bool isMulti() const { return data.getProperty("b", juce::var()).isObject(); }
    juce::String searchBlob() const {
        return (name + " " + type + " " + character + " " + tags.joinIntoString(" ") + " " + style.joinIntoString(" ") + " "
                + genre.joinIntoString(" ") + " " + author + " " + key).toLowerCase(); }
};

class PresetManager {
public:
    juce::Array<PresetInfo> presets;
    juce::StringArray favorites;

    static juce::File userDir() {
        auto d = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Trill Tron").getChildFile("RAYGUN");
        d.getChildFile("User Presets").createDirectory(); return d; }

    void loadAll(const void* factoryJson, size_t size) {
        presets.clear();
        auto parsed = juce::JSON::parse(juce::String::fromUTF8((const char*) factoryJson, (int) size));
        if (auto* arr = parsed.getArray()) for (auto& v : *arr) add(v, true, {});
        // factory presets installed on disk by the installer (optional extras)
        for (auto& f : userDir().getChildFile("User Presets").findChildFiles(juce::File::findFiles, true, "*.rgp")) {
            auto v = juce::JSON::parse(f); if (v.isObject()) add(v, false, f); }
        loadFavorites();
    }
    static PresetInfo parse(const juce::var& v, bool factory, const juce::File& f) {
        PresetInfo p; p.data = v; p.factory = factory; p.file = f;
        p.name = v.getProperty("name", "Untitled").toString(); p.author = v.getProperty("author", "Trill Tron").toString();
        p.type = v.getProperty("type", "FX").toString(); p.character = v.getProperty("character", "").toString();
        p.desc = v.getProperty("desc", "").toString(); p.key = v.getProperty("key", "").toString(); p.bpm = (int) v.getProperty("bpm", 0);
        auto arr = [&](const char* k, juce::StringArray& out) { if (auto* a = v.getProperty(k, juce::var()).getArray()) for (auto& s : *a) out.add(s.toString()); };
        arr("tags", p.tags); arr("style", p.style); arr("genre", p.genre); return p; }
    void add(const juce::var& v, bool factory, const juce::File& f) { presets.add(parse(v, factory, f)); }

    bool isFav(const juce::String& n) const { return favorites.contains(n); }
    void toggleFav(const juce::String& n) { if (favorites.contains(n)) favorites.removeString(n); else favorites.add(n); saveFavorites(); }
    void loadFavorites() { auto f = userDir().getChildFile("favorites.txt"); favorites.clear(); if (f.existsAsFile()) favorites.addLines(f.loadFileAsString()); favorites.removeEmptyStrings(); }
    void saveFavorites() { userDir().getChildFile("favorites.txt").replaceWithText(favorites.joinIntoString("\n")); }

    juce::StringArray allValues(const char* what) const {
        juce::StringArray s; for (auto& p : presets) {
            if (juce::String(what) == "type") s.addIfNotAlreadyThere(p.type);
            else if (juce::String(what) == "character") s.addIfNotAlreadyThere(p.character);
            else if (juce::String(what) == "style") for (auto& t : p.style) s.addIfNotAlreadyThere(t);
            else if (juce::String(what) == "genre") for (auto& t : p.genre) s.addIfNotAlreadyThere(t); }
        return s; }
};
}
