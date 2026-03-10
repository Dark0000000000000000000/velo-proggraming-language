# ⚡ Velo Language

> A friendly scripting language — Lua + BASIC + Python + Go vibes

---

## Build (MSYS2 MinGW64)

```bash
# Open the MSYS2 MinGW64 shell, navigate to the velo/ folder
bash setup_and_build.sh
./build/velo.exe
```

The script automatically installs all dependencies (cmake, ninja, gcc, glfw, mesa) and clones Dear ImGui.

---

## Syntax Reference

### Variables
```velo
let x = 42
let name = "Velo"
let active = true
let nothing = nil
```

### Strings — concat with `..` or `+`
```velo
let s = "Hello, " .. name
let also = "Hi " + name
```

### Lists
```velo
let nums = [1, 2, 3]
nums[0]           -- read by index
nums[0] = 99      -- write by index
push(nums, 4)     -- append
pop(nums)         -- remove & return last
len(nums)         -- length
```

### Functions
```velo
func add(a, b)
    return a + b
end

let result = add(3, 4)  -- 7
```

### If / elif / else
```velo
if x > 10 then
    print("big")
elif x == 10 then
    print("ten")
else
    print("small")
end
```

### For loop
```velo
for i = 1 to 10 do
    print(i)
end

-- custom step
for i = 0 to 100 step 5 do
    print(i)
end

-- countdown
for i = 10 to 1 step -1 do
    print(i)
end
```

### While loop
```velo
let i = 0
while i < 10 do
    i = i + 1
end
```

### break / continue
```velo
for i = 0 to 100 do
    if i == 5 then break end
    if i % 2 == 0 then continue end
    print(i)
end
```

---

## Built-in Functions

| Function | Description |
|---|---|
| `print(a, b...)` | Print values (tab-separated) |
| `input(prompt?)` | Read a line from user |
| `tonum(s)` | String → number |
| `tostr(v)` | Value → string |
| `len(x)` | Length of list or string |
| `push(list, v)` | Append to list |
| `pop(list)` | Remove & return last element |
| `type(v)` | Type name as string |
| `range(n)` / `range(a,b)` / `range(a,b,step)` | Generate list |
| `sqrt(n)` | Square root |
| `floor(n)` / `ceil(n)` | Rounding |
| `abs(n)` | Absolute value |
| `sin(n)` / `cos(n)` | Trigonometry |
| `max(a,b)` / `min(a,b)` | Maximum / minimum |

---

## Operators

| Operator | Description |
|---|---|
| `+` `-` `*` `/` `%` `^` | Arithmetic (^ = power) |
| `..` or `+` | String concatenation |
| `==` `!=` `<` `<=` `>` `>=` | Comparison |
| `and` `or` `not` | Logic |
| `=` | Assignment |

Comments: `-- this is a comment`

---

## Example — Bubble Sort

```velo
let arr = [64, 34, 25, 12, 22, 11, 90]
let n = len(arr)

for i = 0 to n - 2 do
    for j = 0 to n - i - 2 do
        if arr[j] > arr[j + 1] then
            let tmp = arr[j]
            arr[j] = arr[j + 1]
            arr[j + 1] = tmp
        end
    end
end

print("Sorted: " .. tostr(arr))
```
