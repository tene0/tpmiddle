Middle button/trackpoint behavior for ThinkPad TrackPoint Keyboard II when ThinkPad Preferred Scrolling is enabled.

VID 0x17EF, PID 0x60EE (USB)/0x60E1 (BT).

Middle button down/up generates:

- USB: Usage page 0xFFA0, usage ID 1, 3 bytes
- BT: Usage page 0xFF00, usage ID 1, 9 bytes

When middle button is down, moving trackpoint generates usage page 0xFF10, usage ID 1, 3 bytes.

- Button down -
  - USB: 15 00 04
  - BT: 15 00 04 02 00 00 00 00 00
- Button up -
  - USB: 15 00 00
  - BT: 15 00 00 02 00 00 00 00 00
- Vertical movement - 16 00 xx
- Horizontal movement - 16 xx 00

Over BT only, vertical movement only also generates wheel event with data = xx * WHEEL_DELTA.

When middle button is down, moving trackpoint generates either vertical or horizontal movement event depending on dominant direction, never both. Movement events are repeated, repeat rate depends on trackpoint pressure. For vertical movement, down is negative, up is positive. For horizontal movement, left is negative, right is positive. Value starts from 1 and increases with trackpoint pressure.

tpmiddle behavior.

- Middle button down + up within 500 ms with no trackpoint movement produces middle click upon button up.
- If first trackpoint movement while middle button is down is horizontal, produces single button 4/5 click and ignores further horizontal movement.
- Vertical/horizontal trackpoint movement while middle button is down produces wheel events.
