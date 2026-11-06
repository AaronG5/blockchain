import os

from pathlib import Path

SIZE = 789

def main():
   i = 1

   input_file = str(Path(__file__).resolve().parent / "konstitucija.txt")
   output_dir = Path(__file__).resolve().parent / "gen_konst/"

   output_dir.mkdir(parents=True, exist_ok=True)

   with open(input_file, "r", encoding="utf-8") as file:
      lines = file.readlines()

   while i < SIZE*2:
      if i > SIZE:
         output_file = str(output_dir / f"konst_{SIZE}.txt")
      else:
         output_file = str(output_dir / f"konst_{i}.txt")

      with open(output_file, "w", encoding="utf-8") as file:
         file.writelines(lines[:i])

      i *= 2

if __name__ == "__main__":
   main()