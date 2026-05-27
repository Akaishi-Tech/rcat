# Hello, rcat

A demo of *what* this renderer can do. Built with **CMake** and `md4c`.

## Inline styles

You can have **bold**, *italic*, ***both***, ~~strikethrough~~, and `inline code`.
Links work too: visit [example.com](https://example.com) for details.
Autolinks like https://example.org/foo are also recognized.

## Lists

Unordered:

- alpha
- beta has a long line that should wrap nicely at the right column width when the terminal is narrow enough to need it
- gamma
  - nested one
  - nested two
    - deep nesting

Ordered:

1. first
2. second
3. third with a longer description that demonstrates how the hanging indent keeps continuation lines aligned under the text instead of under the marker

Task list:

- [x] write parser bindings
- [x] handle nested lists
- [ ] add image preview over kitty graphics

## Block quote

> "Markdown is intended to be as easy-to-read and easy-to-write as is feasible."
> — John Gruber

Quotes can be nested too:

> outer
>
> > inner
> > with multiple lines

## Code

Inline like `printf("hi\n")`. Fenced block:

```cpp
#include <cstdio>
int main() {
    std::puts("hello from rcat");
    return 0;
}
```

## Horizontal rule

---

## Table

| Feature       | Status | Notes              |
| :------------ | :----: | -----------------: |
| Headings      |   ✓    | 1–6, colored       |
| Lists         |   ✓    | nested, ordered    |
| Tables        |   ✓    | box-drawing chars  |
| Images        |   ~    | shown as alt + URL |

## Images

A local image, rendered with half-block characters:

![rcat logo](logo.png)

A remote image stays as a label since we only handle local paths:

![remote example](https://example.com/logo.png)

That's the end of the demo.
