// Aether - Copyright (C) 2026 The Aether authors. Licensed under the GNU AGPLv3 or later.
//
// The on-screen keyboard.

#include <catch2/catch_test_macros.hpp>

#include "ui/KeyboardPanel.h"

using namespace aether;

TEST_CASE ("The on-screen keys keep their size when the window gets wider", "[ui][keyboard]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const auto theme = ui::Theme::dark();
    ui::KeyboardPanel panel (theme, {});
    auto& keys = panel.getKeys();

    panel.setSize (800, ui::KeyboardPanel::idealHeight);
    CHECK (keys.getKeyWidth() == ui::KeyboardPanel::keyWidth);
    CHECK (keys.getWidth() == 800 - 20);   // fills the panel, scrolling to reach the rest

    // Full screen: same key size, all 88 keys, centred.
    panel.setSize (2560, ui::KeyboardPanel::idealHeight);
    CHECK (keys.getKeyWidth() == ui::KeyboardPanel::keyWidth);
    CHECK (keys.getWidth() < 2560 - 20);
    CHECK (keys.getWidth() >= (int) (52 * ui::KeyboardPanel::keyWidth));
    CHECK (std::abs (keys.getX() - (2560 - keys.getRight())) <= 1);
}
