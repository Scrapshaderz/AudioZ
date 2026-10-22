// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace audioz::ui
{

/** Beschreibung eines Eingabefelds fuer FormDialog. */
struct FormField
{
    enum class Type { Number, Choice, Toggle, Info };

    juce::String id;
    juce::String label;
    Type type = Type::Info;

    // Number
    double value = 0.0;
    double minValue = -1.0e9;
    double maxValue = 1.0e9;
    int decimals = 1;
    juce::String suffix;

    // Choice
    juce::StringArray choices;
    int choiceIndex = 0;

    // Toggle
    bool toggled = false;

    // Info: Text und Hoehe in Pixeln
    juce::String text;
    int height = 44;

    static FormField number (const juce::String& id, const juce::String& label, double value, double minValue,
                             double maxValue, int decimals, const juce::String& suffix = {})
    {
        FormField f;
        f.id = id; f.label = label; f.type = Type::Number; f.value = value;
        f.minValue = minValue; f.maxValue = maxValue; f.decimals = decimals; f.suffix = suffix;
        return f;
    }

    static FormField choice (const juce::String& id, const juce::String& label, const juce::StringArray& choices, int index)
    {
        FormField f;
        f.id = id; f.label = label; f.type = Type::Choice; f.choices = choices; f.choiceIndex = index;
        return f;
    }

    static FormField toggle (const juce::String& id, const juce::String& label, bool on)
    {
        FormField f;
        f.id = id; f.label = label; f.type = Type::Toggle; f.toggled = on;
        return f;
    }

    static FormField info (const juce::String& id, const juce::String& text, int height = 44)
    {
        FormField f;
        f.id = id; f.type = Type::Info; f.text = text; f.height = height;
        return f;
    }
};

/** Inhalt eines Formular-Dialogs. Wird auch an die Rueckrufe uebergeben, damit sich
    Felder und Hinweistexte abhaengig von anderen Eingaben aendern koennen. */
class FormContent : public juce::Component
{
public:
    explicit FormContent (std::vector<FormField> fields, const juce::String& okText, const juce::String& cancelText);
    ~FormContent() override;

    double getNumber (const juce::String& id) const;
    int getChoice (const juce::String& id) const;
    bool getToggle (const juce::String& id) const;

    void setNumber (const juce::String& id, double value);
    void setChoice (const juce::String& id, int index);
    void setInfo (const juce::String& id, const juce::String& text);
    void setFieldEnabled (const juce::String& id, bool enabled);

    /** Wird nach jeder Aenderung eines Felds aufgerufen (Parameter: Feld-ID). */
    std::function<void (FormContent&, const juce::String&)> onChange;

    /** Wird bei OK aufgerufen; das Fenster schliesst sich anschliessend. */
    std::function<void (FormContent&)> onAccept;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    int getIdealHeight() const { return idealHeight; }
    static constexpr int kWidth = 460;

    void closeDialog (int result);

private:
    struct Row;

    Row* find (const juce::String& id) const;

    std::vector<std::unique_ptr<Row>> rows;
    juce::TextButton okButton, cancelButton;
    int idealHeight = 0;
};

/** Oeffnet einen Dialog mit den Feldern. `onAccept` wird bei OK aufgerufen.
    `onChange` (optional) erlaubt abhaengige Felder; es wird einmal direkt nach dem
    Aufbau mit leerer ID aufgerufen. Ohne Abbrechen-Knopf: cancelText leer lassen. */
void showFormDialog (juce::Component* parent, const juce::String& title, std::vector<FormField> fields,
                     const juce::String& okText, const juce::String& cancelText,
                     std::function<void (FormContent&)> onAccept,
                     std::function<void (FormContent&, const juce::String&)> onChange = {});

}  // namespace audioz::ui
