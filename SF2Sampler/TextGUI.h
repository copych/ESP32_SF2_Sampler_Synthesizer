#pragma once
#include "config.h"

#ifdef ENABLE_GUI

#include <U8g2lib.h>
#include <functional>
#include <vector>
#include <memory>
#include "button.h"
#include "encoder.h"
#include "synth.h"
#include "SynthState.h"

// Forward declarations
class U8G2;
class TextGUI;
class MenuItem;
class Synth;
class SynthState;

using CustomDrawFn = std::function<void(TextGUI&, U8G2&, int, int)>; 

// MenuContext declaration
struct MenuContext {
    std::vector<class MenuItem> items;
    String title;
    int parentIndex = -1;
    int scrollPosition = 0;
};

// MenuItemType enum
enum class MenuItemType {
    SUBMENU,
    ACTION,
    TOGGLE,
    VALUE,
    CUSTOM
};

class MenuItem {
public:
   // enum class MenuItemType { ACTION, VALUE, TOGGLE, SUBMENU, CUSTOM };
    using MenuAction = std::function<void(TextGUI&)>;
    using ValueGetter = std::function<int()>;
    using ValueSetter = std::function<void(int)>;
    using MenuGenerator = std::function<std::vector<MenuItem>()>;

    MenuItem();
    MenuItem(const MenuItem&);
    MenuItem& operator=(const MenuItem&);
    MenuItem(MenuItem&&) noexcept;
    MenuItem& operator=(MenuItem&&) noexcept;
    ~MenuItem();

    static MenuItem Action(const String&, MenuAction);
    static MenuItem Value(const String&, ValueGetter, ValueSetter, int, int, int);
    static MenuItem Toggle(const String&, ValueGetter, ValueSetter);
    static MenuItem Submenu(const String&, MenuGenerator);
    static MenuItem Custom(const String&, std::function<void(TextGUI&, U8G2&, int, int)>, MenuAction = nullptr);

    String title;
    MenuItemType type;

    union {
        struct {
            ValueGetter getter;
            ValueSetter setter;
            int min, max, step;
        } value;

        struct {
            MenuGenerator generator;
        } submenu;

        struct {
            MenuAction action;
        } command;

        struct {
            std::function<void(TextGUI&, U8G2&, int, int)> customDraw;
            MenuAction customAction;
        } custom;
    };

    void destroyCurrent();
    void moveFrom(MenuItem&&);
};



class TextGUI {
public:
    TextGUI(Synth& synthRef, SynthState& state);
    void begin();
    void splash();
    void startMenu();
    void process();
    void draw();
    void fullUpdate();
    void busyMessage(const String& str);

    // Navigation methods
    void enterSubmenu(std::vector<MenuItem>&& items, const String& title = "");
    void goBack();
    void refreshCurrentMenu();

    // Access to hardware
    U8G2& getDisplay() { return display; }
    Synth& getSynth() { return synth; }

    // Encoder/button handling , they are public so that they can be handled externally
    uint8_t encA;
    uint8_t encB;
    uint8_t btnState;

private:
    bool inited = false;
    Synth& synth;
    SynthState& state;
    MuxEncoder encoder;
    MuxButton button;

    U8_OBJECT display;
    
    bool editingValue = false;
    // Menu state
    std::vector<MenuContext> menuStack;
    int cursorPos = 0;
    bool needsRedraw = true;
    

    // Initial menu setup
    std::vector<MenuItem> createRootMenu(SynthState& state);

    // Event handlers
    void onEncoderTurn(int direction);
    void onButtonEvent(MuxButton::btnEvents evt);
    
    // Rendering
    void renderDisplay();
    void renderMenu();
    void renderStatusBar();
    int partialDisplayUpdate();
    
    // Value adjustment helpers
    void adjustValue(int direction, MenuItem& item);
};

#endif