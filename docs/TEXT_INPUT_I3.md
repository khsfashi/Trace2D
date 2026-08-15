# UTF-8 Text Input and IME Composition — I3

I3 is the text-entry slice of #72. It adds a real UTF-8/IME boundary without widening the fixed-size `InputEvent` or changing `InputSystem` / `ActionMap` fixed-step semantics.

## Authority boundary

Gameplay controls and text entry are intentionally different domains:

```text
key / mouse / gamepad
  -> Platform
  -> InputEvent
  -> InputSystem
  -> ActionMap
  -> gameplay

native text / IME
  -> Platform
  -> TextInputEvent
  -> focused UiDocument TextInput
```

A `KeyA` press is never converted into the character `a`. Keyboard layout, modifiers, dead keys, IME composition, and committed Unicode text remain platform text-input responsibilities.

`TextInputEvent` is engine-owned and contains no SDL type. It has two event kinds:

- `Committed` — UTF-8 text accepted by the native text system and ready to append to the focused textbox.
- `Composition` — transient IME/preedit text plus platform selection/cursor metadata.

The composition `selectionStart` / `selectionLength` values preserve the backend-provided UTF-8 character indices. `-1` means unavailable. I3 does not reinterpret these values as byte offsets or implement caret editing.

## UI semantics

`UiDocument::ApplyTextInput` is the single semantic delivery point for physical host, direct host, and headless/virtual text events.

Delivery succeeds only when the current focus is a visible, enabled `UiElementKind::TextInput`.

Committed and transient state are separate:

- composition never changes `UiElement::text`,
- a committed event appends to `UiElement::text`,
- commit clears the active composition,
- an empty composition event clears the active composition,
- moving focus to another element clears composition,
- `ClearFocus()` clears composition,
- refocusing the same element does not spuriously clear composition.

I3 keeps one reusable composition `std::string` per `UiDocument`, not one per UI element. There can be only one focused element, so panels, labels, buttons, and inactive text fields do not carry unused composition-string objects.

The existing `UiDocument::InputText(id, text)` contract is preserved for Agent/semantic replacement. It still replaces the complete committed textbox value. A successful replacement clears transient composition so an old IME preedit cannot survive an authoritative semantic edit.

I3 is append-only for physical committed text because caret/navigation editing belongs to #75. It does not pretend to support insertion at an arbitrary caret, selection replacement, Backspace/Delete editing, clipboard, or undo.

## SDL3 platform ownership

`engine/platform` translates:

- `SDL_EVENT_TEXT_INPUT` -> `TextInputEventType::Committed`,
- `SDL_EVENT_TEXT_EDITING` -> `TextInputEventType::Composition`.

SDL event structures are consumed inside Platform and are not exposed to UI/gameplay.

Native text input is window-scoped and explicitly controlled through:

```cpp
platform.SetTextInputEnabled(true);
platform.SetTextInputEnabled(false);
```

The host should enable native text input only while its authoritative UI focus is a visible, enabled text-entry element, and disable it when that focus is lost. This is deliberately not an always-on Platform policy: enabling text input can activate an IME or software keyboard and can affect ordinary key event delivery on some platforms.

A representative host routes events as follows:

```cpp
while (platform.PollEvent(event))
{
    if (event.type == PlatformEventType::Input)
        application.ApplyInput(event.input);
    else if (event.type == PlatformEventType::TextInput)
        application.Ui().ApplyTextInput(event.textInput);
}
```

Headless tests and Agent-style direct injection construct the same `TextInputEvent` and call the same `UiDocument::ApplyTextInput`; no SDL/window/GPU path is required.

## Performance and allocation boundary

I3 deliberately keeps text payloads out of `InputEvent`, `InputSystem`, and `ActionMap`.

Therefore normal button/axis gameplay retains the existing I0-I2 guarantees:

- fixed-index low-level state,
- resolved-index semantic reads,
- no text string storage in the action path,
- no semantic string lookup in `ActionMap::Resolve`,
- no new per-frame allocation in `ActionMap::Resolve`,
- no SDL conversion below Platform.

Text input is an explicit variable-payload boundary and may allocate proportional to entered/preedit UTF-8 bytes. `UiDocument` retains one composition buffer and uses `assign`/`clear` so capacity can be reused across ordinary IME updates. Committed text appends to the textbox's existing `std::string` and follows normal string capacity growth rules.

No filesystem work, font lookup, glyph shaping, renderer upload, device discovery, or action-map rebuild is performed by text delivery.

## Validation expectations

I3 acceptance covers:

- multibyte UTF-8 committed text,
- Korean/CJK byte preservation independent of source-file encoding,
- composition metadata preservation,
- composition not mutating committed text,
- deterministic composition clearing on commit/focus changes/semantic replacement,
- explicit `NotFocused` / `NotTextInput` / invalid-composition results,
- headless native-text activation remaining inert,
- external windowed host compilation through the same `PlatformEvent::TextInput` contract.

## Explicit handoff

I3 does **not** complete #72 and must not advance the core lane to #73 by itself.

Still owned by the remaining #72 closure work:

- pointer presentation -> logical viewport -> world convenience mapping through the existing #88 camera/viewport authority,
- haptics/rumble ownership/support or an explicit defer decision,
- broader device/mobile scope only when the supported platform/workload justifies it.

Later milestones remain separate:

- #74 owns production font assets, glyph coverage, shaping/raster/rendering, including CJK rendering quality.
- #75 owns caret/selection/navigation, editing commands, clipboard, richer focus/input routing, and broader widget behavior.
