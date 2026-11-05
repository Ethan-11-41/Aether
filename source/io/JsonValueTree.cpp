// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
#include "JsonValueTree.h"

namespace aether::json
{

juce::var toVar (const juce::ValueTree& tree)
{
    if (! tree.isValid())
        return {};

    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty (typeKey, tree.getType().toString());

    for (int i = 0; i < tree.getNumProperties(); ++i)
    {
        const auto name = tree.getPropertyName (i);
        jassert (! name.toString().startsWithChar ('_'));   // reserved for the file structure
        object->setProperty (name, tree[name]);
    }

    if (tree.getNumChildren() > 0)
    {
        juce::Array<juce::var> children;

        for (const auto& child : tree)
            children.add (toVar (child));

        object->setProperty (childrenKey, children);
    }

    return juce::var (object.release());
}

juce::ValueTree fromVar (const juce::var& value)
{
    auto* object = value.getDynamicObject();

    if (object == nullptr)
        return {};

    const auto typeName = object->getProperty (typeKey).toString();

    if (typeName.isEmpty() || ! juce::Identifier::isValidIdentifier (typeName))
        return {};

    juce::ValueTree tree { juce::Identifier (typeName) };

    for (const auto& property : object->getProperties())
    {
        const auto key = property.name.toString();

        if (key == typeKey || key == childrenKey)
            continue;

        // Only simple values are allowed as properties; anything else is ignored.
        const auto& v = property.value;
        if (v.isString() || v.isInt() || v.isInt64() || v.isDouble() || v.isBool())
            tree.setProperty (property.name, v, nullptr);
    }

    if (auto* children = object->getProperty (childrenKey).getArray())
        for (const auto& childVar : *children)
            if (auto child = fromVar (childVar); child.isValid())
                tree.appendChild (child, nullptr);

    return tree;
}

juce::String toJsonText (const juce::var& value)
{
    // 17 significant digits guarantee that a decimal number reads back exactly the same.
    return juce::JSON::toString (value, juce::JSON::FormatOptions {}.withSpacing (juce::JSON::Spacing::multiLine)
                                                                    .withMaxDecimalPlaces (17));
}

} // namespace aether::json
