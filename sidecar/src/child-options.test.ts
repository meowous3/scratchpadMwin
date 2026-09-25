import { describe, it, expect } from "vitest";
import { readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";

// Node is a console program started by a GUI app on Windows: every child it
// spawns without windowsHide flashes a console window. Guard every call site.
const SRC = join(__dirname);
const CALL = /\b(execFileAsync|execFile|spawn)\(/g;

function callText(src: string, start: number): string {
  let depth = 0;
  for (let i = start; i < src.length; i++) {
    if (src[i] === "(") depth++;
    else if (src[i] === ")" && --depth === 0) return src.slice(start, i + 1);
  }
  return src.slice(start);
}

describe("child processes", () => {
  const files = [
    ...readdirSync(SRC).map((f) => join(SRC, f)),
    ...readdirSync(join(SRC, "plugins")).map((f) => join(SRC, "plugins", f)),
  ].filter((f) => f.endsWith(".ts") && !f.endsWith(".test.ts"));

  it("all set windowsHide", () => {
    const missing: string[] = [];
    for (const f of files) {
      const src = readFileSync(f, "utf-8");
      for (const m of src.matchAll(CALL)) {
        const text = callText(src, m.index! + m[0].length - 1);
        if (!text.includes("windowsHide")) {
          const line = src.slice(0, m.index).split("\n").length;
          missing.push(`${f.slice(SRC.length + 1)}:${line}`);
        }
      }
    }
    expect(missing).toEqual([]);
  });
});
