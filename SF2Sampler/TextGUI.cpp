#include "config.h"

#ifdef ENABLE_GUI

#include "TextGUI.h"
#include "LoadingProgress.h"
#include "MenuStructure.h"
#include <SPI.h>
#include <functional>
#include <new> 
#include <Wire.h>


MenuItem::MenuItem() {
    type = MenuItemType::ACTION;
    new (&command.action) MenuAction(nullptr);
}

MenuItem::MenuItem(MenuItem&& other) noexcept {
    moveFrom(std::move(other));
}


// Copy constructor
MenuItem::MenuItem(const MenuItem& other) {
    title = other.title;
    type = other.type;
    showBusy = other.showBusy;
    switch (type) {
        case MenuItemType::VALUE:
        case MenuItemType::TOGGLE:
            new (&value.getter) ValueGetter(other.value.getter);
            new (&value.setter) ValueSetter(other.value.setter);
            value.min = other.value.min;
            value.max = other.value.max;
            value.step = other.value.step;
            break;
        case MenuItemType::SUBMENU:
            new (&submenu.generator) MenuGenerator(other.submenu.generator);
            break;
        case MenuItemType::ACTION:
            new (&command.action) MenuAction(other.command.action);
            break;
        case MenuItemType::CUSTOM:
            new (&custom.customDraw) decltype(custom.customDraw)(other.custom.customDraw);
            new (&custom.customAction) MenuAction(other.custom.customAction);
            break;
    }
}



// Copy assignment operator
MenuItem& MenuItem::operator=(const MenuItem& other) {
    if (this != &other) {
        destroyCurrent();
        new (this) MenuItem(other);
    }
    return *this;
}
// Move -"-
MenuItem& MenuItem::operator=(MenuItem&& other) noexcept {
    if (this != &other) {
        destroyCurrent();
        moveFrom(std::move(other));
    }
    return *this;
}


MenuItem::~MenuItem() {
    destroyCurrent();
}

void MenuItem::destroyCurrent() {
    switch (type) {
        case MenuItemType::VALUE:
        case MenuItemType::TOGGLE:
            value.getter.~ValueGetter();
            value.setter.~ValueSetter();
            break;
        case MenuItemType::SUBMENU:
            submenu.generator.~MenuGenerator();
            break;
        case MenuItemType::ACTION:
            command.action.~MenuAction();
            break;
        case MenuItemType::CUSTOM: 
            custom.customDraw.~CustomDrawFn();
            custom.customAction.~MenuAction();
            break;
    }
}



void MenuItem::moveFrom(MenuItem&& other) {
    title = std::move(other.title);
    type = other.type;
    showBusy = other.showBusy;

    switch (type) {
        case MenuItemType::VALUE:
        case MenuItemType::TOGGLE:
            new (&value.getter) ValueGetter(std::move(other.value.getter));
            new (&value.setter) ValueSetter(std::move(other.value.setter));
            value.min = other.value.min;
            value.max = other.value.max;
            value.step = other.value.step;
            break;
        case MenuItemType::SUBMENU:
            new (&submenu.generator) MenuGenerator(std::move(other.submenu.generator));
            break;
        case MenuItemType::ACTION:
            new (&command.action) MenuAction(std::move(other.command.action));
            break;
        case MenuItemType::CUSTOM:
            new (&custom.customDraw) decltype(custom.customDraw)(std::move(other.custom.customDraw));
            new (&custom.customAction) MenuAction(std::move(other.custom.customAction));
            break;
    }

    // Safe fallback for moved-from
    other.type = MenuItemType::ACTION;
    new (&other.command.action) MenuAction(nullptr);
}


// Factory methods
MenuItem MenuItem::Submenu(const String& title, MenuGenerator generator, bool showBusy) {
    MenuItem item{};
    item.title = title;
    item.type = MenuItemType::SUBMENU;
    item.showBusy = showBusy;
    new (&item.submenu.generator) MenuGenerator(std::move(generator));
    return item;
}

MenuItem MenuItem::Action(const String& title, MenuAction action) {
    MenuItem item{};
    item.title = title;
    item.type = MenuItemType::ACTION;
    new (&item.command.action) MenuAction(std::move(action));
    return item;
}

MenuItem MenuItem::Toggle(const String& title, ValueGetter getter, ValueSetter setter) {
    MenuItem item;
    item.title = title;
    item.type = MenuItemType::TOGGLE;

    new (&item.value.getter) ValueGetter(std::move(getter));
    new (&item.value.setter) ValueSetter(std::move(setter));

    item.value.min = 0;
    item.value.max = 1;
    item.value.step = 1;

    return item;
}

MenuItem MenuItem::Value(const String& title, ValueGetter getter, ValueSetter setter,
                         int min, int max, int step) {
    MenuItem item;
    item.title = title;
    item.type = MenuItemType::VALUE;

    // Proper placement new construction for std::function inside union
    new (&item.value.getter) ValueGetter(std::move(getter));
    new (&item.value.setter) ValueSetter(std::move(setter));

    item.value.min = min;
    item.value.max = max;
    item.value.step = step;

    return item;
}


MenuItem MenuItem::Custom(const String& title, 
                        std::function<void(TextGUI&, U8G2&, int, int)> drawFn,
                        MenuAction action) {
    MenuItem item{};
    item.title = title;
    item.type = MenuItemType::CUSTOM;
    new (&item.custom.customDraw) decltype(item.custom.customDraw)(std::move(drawFn));
    if (action) {
        new (&item.custom.customAction) MenuAction(std::move(action));
    }
    return item;
}

// ==============================================================================================================================================================

TextGUI::TextGUI(Synth& synthRef, SynthState& stateRef) :
      synth(synthRef)
    , state(stateRef)
    , display(U8_INIT_ARGS)
    , encA(0), encB(0), btnState(0)
    {}


void TextGUI::begin() {

    pinMode(ENC0_A_PIN, SIG_INPUT_MODE);
    pinMode(ENC0_B_PIN, SIG_INPUT_MODE);
    pinMode(BTN0_PIN, SIG_INPUT_MODE);

#if defined(DISPLAY_INTERFACE_HW_I2C)

    // HW I2C: peripheral controls pins
    Wire.begin(DISPLAY_SDA, DISPLAY_SCL);

#elif defined(DISPLAY_INTERFACE_SW_I2C)

    // SW I2C: u8g2 bitbang > we must drive pins
    pinMode(DISPLAY_SDA, OUTPUT);
    pinMode(DISPLAY_SCL, OUTPUT);
    digitalWrite(DISPLAY_SDA, HIGH);
    digitalWrite(DISPLAY_SCL, HIGH);

#elif defined(DISPLAY_INTERFACE_HW_SPI)

    // HW SPI: init bus (use custom pins if needed)
    // SPI.begin(); 
    // or:
    SPI.begin(DISPLAY_SCL, U8X8_PIN_NONE, DISPLAY_SDA, DISPLAY_CS);

#elif defined(DISPLAY_INTERFACE_SW_SPI)

    // SW SPI: u8g2 bitbang > we must drive pins
    pinMode(DISPLAY_SCL, OUTPUT);   // CLK
    pinMode(DISPLAY_SDA, OUTPUT);   // MOSI
    pinMode(DISPLAY_CS, OUTPUT);
    pinMode(DISPLAY_DC, OUTPUT);
    if (DISPLAY_RES != U8X8_PIN_NONE)
        pinMode(DISPLAY_RES, OUTPUT);

    digitalWrite(DISPLAY_SCL, LOW);
    digitalWrite(DISPLAY_SDA, LOW);

#endif

    display.begin();

    display.setContrast(255);
    display.setFont(u8g2_font_6x12_m_symbols);
    display.enableUTF8Print();
    display.setDrawColor(2);
    display.setFontPosTop();

    inited = true;
}

void TextGUI::startMenu() {
    if (!inited) return;

    encoder.bind(0, &encA, &encB, [this](int, int dir) {
        this->onEncoderTurn(dir);
    }, MuxEncoder::MODE_QUAD_STEP);

    button.bind(0, &btnState, [this](int, MuxButton::btnEvents evt) {
        this->onButtonEvent(evt);
    });

    enterSubmenu(MenuStructure::createRootMenu(synth, state), "Main Menu"); 
}

void TextGUI::process() {
    if (busy) return;
    encoder.process();
    button.process();

}

void TextGUI::draw() {
    if (busy || !inited) return;

    // Observe a monotonic revision rather than only active/value. A short
    // Program Change can begin and finish between two draw() calls; in that
    // case the old code never saw active=true and left the 100% loading frame
    // on the OLED. finish() increments revision, so completion always causes
    // the normal MIDI activity meter to be rendered again.
    const uint32_t loadingRevision = LoadingProgress::getRevision();
    if (loadingRevision != lastLoadingRevision) {
        lastLoadingRevision = loadingRevision;
        lastLoadingProgress = LoadingProgress::isActive() ? LoadingProgress::get() : 0xFF;
        needsRedraw = true;
    }

    // MIDI activity is external GUI state: NoteOn raises it and ControlTask
    // periodically decays it.  The event-driven GUI therefore has to notice
    // when one of the 16 *visible* bar levels changes.  Compare the same
    // quantized levels used by renderStatusBar(), so we redraw only when the
    // OLED representation actually changes.
    if (!LoadingProgress::isActive()) {
        char activity[49];
        synth.getActivityString(activity);
        bool activityChanged = !activityLevelsValid;

        for (int i = 0; i < 16; ++i) {
            uint8_t level = (uint8_t)activity[i * 3 + 2] - 0x81;
            if (level > 7) level = 7;
            if (!activityLevelsValid || level != lastActivityLevels[i]) {
                lastActivityLevels[i] = level;
                activityChanged = true;
            }
        }

        activityLevelsValid = true;
        if (activityChanged) needsRedraw = true;
    } else {
        // Force a fresh comparison when loading ends, even if MIDI activity
        // happened to return to the same levels seen before loading.
        activityLevelsValid = false;
    }

    // Render the framebuffer only when GUI state changed. If a new redraw
    // request arrives while a previous tiled transfer is in progress, restart
    // the transfer from the top-left so the OLED receives one coherent frame.
    if (needsRedraw) {
        renderDisplay();
        needsRedraw = false;
        updateTileX = 0;
        updateTileY = 0;
        displayUpdateInProgress = true;
    }

    if (displayUpdateInProgress && partialDisplayUpdate() == 0) {
        displayUpdateInProgress = false;
    }
}

void TextGUI::renderDisplay() {
    display.clearBuffer();
    renderMenu();
    renderStatusBar();
}

void TextGUI::fullUpdate() {
    if (!inited) return;
    display.clearBuffer();
    renderMenu();
    renderStatusBar();
    display.sendBuffer();
    needsRedraw = false;
    displayUpdateInProgress = false;
    updateTileX = 0;
    updateTileY = 0;
}


void TextGUI::renderMenu() {
    if (menuStack.empty()) return;
       
    auto& current = menuStack.back();
    const uint8_t lineHeight = 10;
    const uint8_t maxVisible = 4;

    // Update scroll position based on cursor movement
    if (cursorPos < current.scrollPosition) {
        current.scrollPosition = cursorPos;
    } else if (cursorPos >= current.scrollPosition + maxVisible) {
        current.scrollPosition = cursorPos - maxVisible + 1;
    }

    // Clamp scrollPosition
    int total = current.items.size();
    if (current.scrollPosition > total - maxVisible) {
        current.scrollPosition = std::max(0, total - maxVisible);
    }

    int start = current.scrollPosition;

    
    // Draw title if available
    uint8_t y = 0;
    if (!current.title.isEmpty()) {
        display.drawUTF8(0, y, current.title.c_str());
        y += lineHeight;
    }
    
    // Draw visible items
    for (int i = start; i < current.items.size() && i < start + maxVisible; i++) {
        const auto& item = current.items[i];
        
        // Cursor indicator
        if (i == cursorPos) {
            display.drawStr(0, y, ">");
        }
        
        // Item rendering
        switch (item.type) {
            case MenuItemType::TOGGLE:
                display.drawUTF8(8, y, item.title ? item.title.c_str() : "");
                display.drawUTF8(display.getDisplayWidth() - display.getUTF8Width("[X]"), y, 
                                 item.value.getter() ? "[X]" : "[ ]");
                break;
                
            case MenuItemType::VALUE: {
                int value = item.value.getter();
                display.drawUTF8(8, y,item.title ? item.title.c_str() : "");
                String valStr = (i == cursorPos && editingValue) ? ">" + String(value) + "<" : " " + String(value) + " ";
                display.drawUTF8(display.getDisplayWidth() - display.getUTF8Width(valStr.c_str()), 
                               y, valStr.c_str());
                break;
            }
                
            case MenuItemType::CUSTOM:
                if (item.custom.customDraw) {
                    item.custom.customDraw(*this, display, 8, y);
                } else {
                    display.drawUTF8(8, y, item.title ? item.title.c_str() : "");
                }
                break;
                
            default:
                display.drawUTF8(8, y, item.title ? item.title.c_str() : "");
                if (item.type == MenuItemType::SUBMENU) {
                    display.drawUTF8(display.getDisplayWidth() - 8, y, ">");
                }
                break;
        }
        
        y += lineHeight;
    }
}

void TextGUI::renderStatusBar() {
    if (!inited) return;

    // Both modes use exactly the same 16 x 6-pixel cells. This keeps the
    // loading indicator visually identical in width, spacing and centering to
    // the MIDI activity meter instead of switching to a wider bar geometry.
    constexpr int cells = 16;
    constexpr int cellW = 6;
    constexpr int barW = 5;
    constexpr int maxH = 7;
    const int x0 = (display.getDisplayWidth() - cells * cellW) / 2;
    const int yBottom = display.getDisplayHeight() - 2;

    if (LoadingProgress::isActive()) {
        const uint16_t units = ((uint16_t)LoadingProgress::get() * cells * maxH + 254u) / 255u;
        for (int i = 0; i < cells; ++i) {
            int h = (int)units - i * maxH;
            if (h < 0) h = 0;
            if (h > maxH) h = maxH;
            if (h) display.drawBox(x0 + i * cellW, yBottom - h + 1, barW, h);
        }
        return;
    }

    // Decode the same seven activity levels that getActivityString() used to
    // encode as Unicode block glyphs, then draw them with the shared geometry.
    char buf[49];
    synth.getActivityString(buf);
    for (int i = 0; i < cells; ++i) {
        int h = (uint8_t)buf[i * 3 + 2] - 0x81;
        if (h < 0) h = 0;
        if (h > maxH) h = maxH;
        if (h) display.drawBox(x0 + i * cellW, yBottom - h + 1, barW, h);
    }
}

void TextGUI::renderLoadingMeter(uint8_t progress) {
    constexpr int cells = 16;
    constexpr int cellW = 6;
    constexpr int barW = 5;
    constexpr int maxH = 7;
    const int x0 = (display.getDisplayWidth() - cells * cellW) / 2;
    const int yBottom = display.getDisplayHeight() - 2;
    const uint16_t units = ((uint16_t)progress * cells * maxH + 254u) / 255u;

    for (int i = 0; i < cells; ++i) {
        int h = (int)units - i * maxH;
        if (h < 0) h = 0;
        if (h > maxH) h = maxH;
        if (h) display.drawBox(x0 + i * cellW, yBottom - h + 1, barW, h);
    }
}

void TextGUI::enterSubmenu(std::vector<MenuItem>&& items, const String& title) {
    MenuContext newContext;
    newContext.items = std::move(items); // Move instead of copy
    newContext.title = title;
    newContext.parentIndex = menuStack.empty() ? -1 : menuStack.size() - 1;
    menuStack.push_back(std::move(newContext));
    cursorPos = 0;
    needsRedraw = true;
}

void TextGUI::busyMessage(const String& str) {
    if (!inited) return;
    display.clearBuffer();
    display.drawUTF8(0, display.getDisplayHeight() / 2, str.c_str());
    display.sendBuffer();
    displayUpdateInProgress = false;
    updateTileX = 0;
    updateTileY = 0;
}

void TextGUI::beginBusy(const String& str) {
    busy = true;
    busyMessage(str);
}

void TextGUI::loadingPumpThunk(void* ctx, uint8_t progress) {
    static_cast<TextGUI*>(ctx)->serviceLoadingProgress(progress);
}

void TextGUI::beginLoading(const String& str) {
    busy = true;
    loadingMessage = str;
    lastLoadingProgress = 0xFF;
    LoadingProgress::setPump(&TextGUI::loadingPumpThunk, this);
    busyMessage(str);
}

void TextGUI::serviceLoadingProgress(uint8_t progress) {
    if (!inited || progress == lastLoadingProgress) return;
    lastLoadingProgress = progress;
    display.clearBuffer();
    display.drawUTF8(0, display.getDisplayHeight() / 2, loadingMessage.c_str());
    renderLoadingMeter(progress);
    display.sendBuffer();
}

void TextGUI::endLoading() {
    LoadingProgress::clearPump();
    LoadingProgress::finish();
    loadingMessage = String();
    lastLoadingProgress = 0xFF;
    busy = false;
    needsRedraw = true;
}

void TextGUI::endBusy() {
    busy = false;
    needsRedraw = true;
}

void TextGUI::goBack() {
    if (menuStack.size() > 1) {
        menuStack.pop_back();
        cursorPos = menuStack.back().scrollPosition;
        needsRedraw = true;
    }
}

void TextGUI::refreshCurrentMenu() {
    if (!menuStack.empty() && !menuStack.back().items.empty()) {
        auto& current = menuStack.back();
        if (current.items[0].type == MenuItemType::SUBMENU && current.items[0].submenu.generator) {
            current.items = current.items[0].submenu.generator();
            needsRedraw = true;
        }
    }
}

void TextGUI::onButtonEvent(MuxButton::btnEvents evt) {
    if (menuStack.empty()) return;
    
    auto& current = menuStack.back();
    if (cursorPos < 0 || cursorPos >= current.items.size()) return;
    
    auto& item = current.items[cursorPos];
    
    if (evt == MuxButton::EVENT_CLICK) {
        switch (item.type) {
            case MenuItemType::SUBMENU:
                if (item.submenu.generator) {
                    const bool showBusy = item.showBusy;
                    if (showBusy) beginBusy("Scanning...");
                    auto items = item.submenu.generator();
                    enterSubmenu(std::move(items), item.title);
                    if (showBusy) endBusy();
                }
                break;
                
            case MenuItemType::ACTION:
                if (item.command.action) {
                    item.command.action(*this);
                }
                break;
                
            case MenuItemType::TOGGLE:
                item.value.setter(!item.value.getter());
                needsRedraw = true;
                break;

            case MenuItemType::VALUE:
                editingValue = !editingValue;  // Toggle edit mode
                needsRedraw = true;
                break;

            case MenuItemType::CUSTOM:
                if (item.custom.customAction) {
                    item.custom.customAction(*this);
                }
                break;
                
            default:
                break;
        }
    } else if (evt == MuxButton::EVENT_LONGPRESS) {
        goBack();
    }
}

void TextGUI::onEncoderTurn(int direction) {
    if (menuStack.empty()) return;
    
    auto& current = menuStack.back();
    if (current.items.empty()) return;
    
    // Handle value adjustment for value-type items
    if (cursorPos >= 0 && cursorPos < current.items.size()) {
        auto& item = current.items[cursorPos];
        if (item.type == MenuItemType::VALUE && editingValue) {
            adjustValue(direction, item);
            return;
        }
    }
    
    // Regular navigation
    cursorPos = constrain(cursorPos + direction, 0, static_cast<int>(current.items.size()) - 1);
    
    // Update scroll position if needed
    if (cursorPos < current.scrollPosition) {
        current.scrollPosition = cursorPos;
    } else if (cursorPos >= current.scrollPosition + 6) {
        current.scrollPosition = cursorPos - 5;
    }
    
    needsRedraw = true;
}

void TextGUI::adjustValue(int direction, MenuItem& item) {
    if (item.type != MenuItemType::VALUE || !item.value.setter) return;
    
    int current = item.value.getter();
    int newValue = current + (direction * item.value.step);
    newValue = constrain(newValue, item.value.min, item.value.max);
    
    if (newValue != current) {
        item.value.setter(newValue);
        needsRedraw = true;
        ESP_LOGI("TextGUI", "Value adjusted to: %d :", newValue);
    }

    ESP_LOGI("TextGUI", "New value: %d current %d max %d min %d", newValue, current, item.value.max, item.value.min);
}

int TextGUI::partialDisplayUpdate() {
    static const int send_tiles = 4;
    const int block_h = display.getBufferTileHeight();
    const int block_w = display.getBufferTileWidth();

    if (updateTileY >= block_h) {
        updateTileX = 0;
        updateTileY = 0;
        return 0;
    }

    const int remaining = block_w - updateTileX;
    const int tiles = (remaining < send_tiles) ? remaining : send_tiles;
    display.updateDisplayArea(updateTileX, updateTileY, tiles, 1);

    updateTileX += tiles;
    if (updateTileX >= block_w) {
        updateTileX = 0;
        ++updateTileY;
    }

    if (updateTileY >= block_h) {
        updateTileX = 0;
        updateTileY = 0;
        return 0;
    }

    return 1;
}

#endif
