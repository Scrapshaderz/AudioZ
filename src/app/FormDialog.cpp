// AudioZ - Audio-Editor und Mastering-Werkzeug
// Copyright (C) 2026 AudioZ-Mitwirkende
// SPDX-License-Identifier: GPL-3.0-or-later
#include "FormDialog.h"

#include "Formatting.h"
#include "Theme.h"

#include <algorithm>

namespace audioz::ui
{

namespace
{
    constexpr int kRowHeight = 34;
    constexpr int kLabelWidth = 170;
    constexpr int kMargin = 20;
    constexpr int kButtonRowHeight = 60;

    double parseNumber (const juce::String& text, double fallback)
    {
        auto cleaned = text.trim().replaceCharacter (',', '.').retainCharacters ("0123456789.-+eE");
        if (cleaned.isEmpty() || ! (cleaned.containsAnyOf ("0123456789")))
            return fallback;

        return cleaned.getDoubleValue();
    }
}

struct FormContent::Row
{
    FormField field;
    std::unique_ptr<juce::Label> label;
    std::unique_ptr<juce::Label> numberEditor;
    std::unique_ptr<juce::Label> suffix;
    std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<juce::Label> info;
    double number = 0.0;
    int height = kRowHeight;
};

FormContent::FormContent (std::vector<FormField> fields, const juce::String& okText, const juce::String& cancelText)
{
    int y = kMargin;

    for (auto& f : fields)
    {
        auto row = std::make_unique<Row>();
        row->field = f;
        row->height = f.type == FormField::Type::Info ? f.height : kRowHeight;
        auto* r = row.get();
        const auto id = f.id;

        auto notify = [this, id] { if (onChange) onChange (*this, id); };

        if (f.type != FormField::Type::Info && f.type != FormField::Type::Toggle)
        {
            r->label = std::make_unique<juce::Label>();
            r->label->setText (f.label, juce::dontSendNotification);
            addAndMakeVisible (*r->label);
        }

        switch (f.type)
        {
            case FormField::Type::Number:
            {
                r->number = juce::jlimit (f.minValue, f.maxValue, f.value);
                r->numberEditor = std::make_unique<juce::Label>();
                r->numberEditor->setEditable (true);
                r->numberEditor->setJustificationType (juce::Justification::centredRight);
                r->numberEditor->setText (formatNumber (r->number, f.decimals), juce::dontSendNotification);
                r->numberEditor->setColour (juce::Label::backgroundColourId, findColour (juce::ComboBox::backgroundColourId));
                r->numberEditor->setColour (juce::Label::outlineColourId, findColour (juce::ComboBox::outlineColourId));
                r->numberEditor->setBorderSize (juce::BorderSize<int> (2, 8, 2, 8));
                r->numberEditor->onTextChange = [this, r, notify]
                {
                    double parsed = parseNumber (r->numberEditor->getText(), r->number);
                    if (r->field.decimals == 0)
                        parsed = std::round (parsed);

                    r->number = juce::jlimit (r->field.minValue, r->field.maxValue, parsed);
                    r->numberEditor->setText (formatNumber (r->number, r->field.decimals), juce::dontSendNotification);
                    notify();
                };
                addAndMakeVisible (*r->numberEditor);

                if (f.suffix.isNotEmpty())
                {
                    r->suffix = std::make_unique<juce::Label>();
                    r->suffix->setText (f.suffix, juce::dontSendNotification);
                    r->suffix->setColour (juce::Label::textColourId, juce::Colours::grey);
                    addAndMakeVisible (*r->suffix);
                }
                break;
            }

            case FormField::Type::Choice:
            {
                r->combo = std::make_unique<juce::ComboBox>();
                for (int i = 0; i < f.choices.size(); ++i)
                    r->combo->addItem (f.choices[i], i + 1);
                r->combo->setSelectedItemIndex (f.choiceIndex, juce::dontSendNotification);
                r->combo->onChange = notify;
                addAndMakeVisible (*r->combo);
                break;
            }

            case FormField::Type::Toggle:
            {
                r->toggle = std::make_unique<juce::ToggleButton> (f.label);
                r->toggle->setToggleState (f.toggled, juce::dontSendNotification);
                r->toggle->onClick = notify;
                addAndMakeVisible (*r->toggle);
                break;
            }

            case FormField::Type::Info:
            {
                r->info = std::make_unique<juce::Label>();
                r->info->setText (f.text, juce::dontSendNotification);
                r->info->setJustificationType (juce::Justification::topLeft);
                r->info->setMinimumHorizontalScale (1.0f);
                r->info->setFont (juce::Font (juce::FontOptions (14.0f)));
                r->info->setBorderSize (juce::BorderSize<int> (0));
                addAndMakeVisible (*r->info);
                break;
            }
        }

        y += r->height + 6;
        rows.push_back (std::move (row));
    }

    okButton.setButtonText (okText);
    okButton.setColour (juce::TextButton::buttonColourId, findColour (juce::TextButton::buttonOnColourId));
    okButton.setColour (juce::TextButton::textColourOffId, findColour (juce::TextButton::textColourOnId));
    okButton.onClick = [this]
    {
        // Eingabe eines noch offenen Felds uebernehmen
        if (onAccept)
            onAccept (*this);

        closeDialog (1);
    };
    addAndMakeVisible (okButton);

    cancelButton.setButtonText (cancelText);
    cancelButton.onClick = [this] { closeDialog (0); };
    addChildComponent (cancelButton);
    cancelButton.setVisible (cancelText.isNotEmpty());

    idealHeight = y + kButtonRowHeight;
    setSize (kWidth, idealHeight);
    setWantsKeyboardFocus (true);
}

FormContent::~FormContent() = default;

FormContent::Row* FormContent::find (const juce::String& id) const
{
    for (auto& r : rows)
        if (r->field.id == id)
            return r.get();

    return nullptr;
}

double FormContent::getNumber (const juce::String& id) const
{
    auto* r = find (id);
    return r != nullptr ? r->number : 0.0;
}

int FormContent::getChoice (const juce::String& id) const
{
    auto* r = find (id);
    return r != nullptr && r->combo != nullptr ? r->combo->getSelectedItemIndex() : 0;
}

bool FormContent::getToggle (const juce::String& id) const
{
    auto* r = find (id);
    return r != nullptr && r->toggle != nullptr && r->toggle->getToggleState();
}

void FormContent::setNumber (const juce::String& id, double value)
{
    if (auto* r = find (id); r != nullptr && r->numberEditor != nullptr)
    {
        r->number = juce::jlimit (r->field.minValue, r->field.maxValue, value);
        r->numberEditor->setText (formatNumber (r->number, r->field.decimals), juce::dontSendNotification);
    }
}

void FormContent::setChoice (const juce::String& id, int index)
{
    if (auto* r = find (id); r != nullptr && r->combo != nullptr)
        r->combo->setSelectedItemIndex (index, juce::dontSendNotification);
}

void FormContent::setInfo (const juce::String& id, const juce::String& text)
{
    if (auto* r = find (id); r != nullptr && r->info != nullptr)
        r->info->setText (text, juce::dontSendNotification);
}

void FormContent::setFieldEnabled (const juce::String& id, bool enabled)
{
    if (auto* r = find (id))
    {
        for (juce::Component* c : { (juce::Component*) r->numberEditor.get(), (juce::Component*) r->combo.get(),
                                    (juce::Component*) r->toggle.get(), (juce::Component*) r->label.get(),
                                    (juce::Component*) r->suffix.get() })
            if (c != nullptr)
                c->setEnabled (enabled);
    }
}

void FormContent::paint (juce::Graphics& g)
{
    g.fillAll (findColour (juce::ResizableWindow::backgroundColourId));
}

void FormContent::resized()
{
    auto area = getLocalBounds().reduced (kMargin, 0);
    area.removeFromTop (kMargin);

    for (auto& r : rows)
    {
        auto line = area.removeFromTop (r->height);
        area.removeFromTop (6);

        if (r->info != nullptr)
        {
            r->info->setBounds (line);
            continue;
        }

        if (r->toggle != nullptr)
        {
            r->toggle->setBounds (line);
            continue;
        }

        r->label->setBounds (line.removeFromLeft (kLabelWidth));

        if (r->numberEditor != nullptr)
        {
            const int suffixWidth = r->suffix != nullptr ? 80 : 0;
            if (r->suffix != nullptr)
                r->suffix->setBounds (line.removeFromRight (suffixWidth).withTrimmedLeft (8));
            r->numberEditor->setBounds (line.removeFromLeft (150));
        }
        else if (r->combo != nullptr)
        {
            r->combo->setBounds (line);
        }
    }

    auto buttons = getLocalBounds().removeFromBottom (kButtonRowHeight).reduced (kMargin, 12);
    okButton.setBounds (buttons.removeFromRight (120));
    buttons.removeFromRight (10);
    cancelButton.setBounds (buttons.removeFromRight (120));
}

bool FormContent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey)
    {
        okButton.triggerClick();
        return true;
    }

    return false;
}

void FormContent::closeDialog (int result)
{
    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
        window->exitModalState (result);
}

void showFormDialog (juce::Component* parent, const juce::String& title, std::vector<FormField> fields,
                     const juce::String& okText, const juce::String& cancelText,
                     std::function<void (FormContent&)> onAccept,
                     std::function<void (FormContent&, const juce::String&)> onChange)
{
    auto content = std::make_unique<FormContent> (std::move (fields), okText, cancelText);
    content->onAccept = std::move (onAccept);
    content->onChange = std::move (onChange);

    if (content->onChange)
        content->onChange (*content, {});

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = title;
    options.dialogBackgroundColour = content->findColour (juce::ResizableWindow::backgroundColourId);
    options.content.setOwned (content.release());
    options.componentToCentreAround = parent;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    options.launchAsync();
}

}  // namespace audioz::ui
