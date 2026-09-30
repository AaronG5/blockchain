# Image-based hash function

## Description

This is a custom hashing function which encrypts input with the help of raw image pixel data.

## How does it work

A PNG image file must be present in the same directory as the executable file. The file must be named `image.png`

### Input

Text or filename (must have the `.txt` extension). Input is expected to only include ASCII symbols. Text inputted by hand is stripped of the newline character.

### Input limitations

Input should not be larger than 128 characters long.

### Step-by-step process:

1. PNG file is read into memory in the form of raw pixel data.
2. The input content is converted into a `int` array (`char[] -> int[]`).
3. The array is summed into a single `int` value, which is used to offset the starting point of the X and Y coordinate pixel lookup in step 4.
4. The array is processed in groups of four elements, with each group split into two pairs. The product of each pair is calculated, an offset is added and used as the X and Y coordinates of the corresponding pixel in the image. If the X or Y coordinate exceeds the image resolution, the coordinate is wrapped using the modulo operation. The quotient obtained by dividing the original coordinate by the image resolution is then added to the resulting coordinate.
5. The RGB values of said pixel undergo bitwise XOR operations: `R ^ G ^ B`.
6. The result of these operations is converted into hexadecimal format and appended to the hash string.
7. If the hash hasn't reached 64 symbol size, the pixel lookup is run again, but the `int` array is offset by the cycle iteration count.
8. If the hash is longer than 64 hex symbols, it is compressed to the appropriate size (64 symbols).
9. The hash is returned

# Running the Hashing function (MacOS or Linux)

From the `Project_1` directory:

```
make build
./hasher
```

# Eksperimentinis Tyrimas: teisingumas ir sparta
