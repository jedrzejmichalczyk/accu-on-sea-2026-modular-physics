# Deck workflow

The slide deck — `ACCU-Separation-of-Concerns-at-Zero-Cost.pptx` — **is the
master**. Edit it by hand in PowerPoint as much as you like: photos, wording,
layout, animations, new slides. Nothing regenerates it from scratch.

The one exception is the **code on the code slides**. That comes from a single
source, `scripts/code_snippets.py`, and is pushed into the deck *surgically* by
`scripts/update_code_slides.py`, which rewrites **only** the code boxes and
leaves everything else exactly as you left it.

## To change a code slide

1. Edit the fragment in [`scripts/code_snippets.py`](../scripts/code_snippets.py)
   (keys map to the C++ in `core/` and `physics/coupled_oscillator/` — keep them
   in sync with the real headers).
2. **Close the deck in PowerPoint** (an open file is locked and can't be written).
3. Run the updater:
   ```sh
   python scripts/update_code_slides.py            # rewrites the repo deck in place
   python scripts/update_code_slides.py --check     # or: just list the code boxes
   ```
4. Re-export the PDF (see below).

The updater finds boxes named `code:<tag>` (e.g. `code:spring`) and re-renders
them with the same syntax highlighting as the original. It preserves each box's
**position and font size**, so any resizing you did by hand sticks — only the
text content and colours are replaced. `highlight_lines` in the snippet file
controls which lines are drawn bold/yellow.

### Don't break the link
The updater locates code by the shape **name** `code:<tag>`. In PowerPoint you
can move, resize, restyle, or recolour a code box freely. Just don't *delete and
recreate* one — that loses the name, and the updater will report the snippet as
"no matching box". (You can re-tag a shape via Home → Select → Selection Pane.)

## Re-exporting the PDF

There's no LibreOffice CLI on this machine, so the PDF is exported via
PowerPoint. Either **File → Export → Create PDF/XPS** in the app, or from
PowerShell:

```powershell
$pp = New-Object -ComObject PowerPoint.Application
$pres = $pp.Presentations.Open("$PWD\slides\ACCU-Separation-of-Concerns-at-Zero-Cost.pptx", -1, 0, 0)
$pres.SaveAs("$PWD\slides\ACCU-Separation-of-Concerns-at-Zero-Cost.pdf", 32)  # 32 = ppSaveAsPDF
$pres.Close(); $pp.Quit()
```

## Where the deck originally came from

The full deck was first built from scratch by `assets/make_deck.py` in the
parent project (`C:\Users\j.michalczyk\Projekty\accu`). That generator is now
**frozen** — it's only for a from-scratch rebuild, which you shouldn't need.
Day-to-day, the `.pptx` here is the master and `code_snippets.py` owns the code.
`make_deck.py` is not tracked in this repo.
