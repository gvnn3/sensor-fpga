# Prompt Log — sensor-fpga

All user prompts given to the coding agent, recorded verbatim in reverse
chronological order (newest first). Each entry has the date and time
(HH:MM:SS) when the prompt was received.

---

## 5 Jul 2026 04:58:12

> FreeRTOS is fine fro this application.

---

## 5 Jul 2026 04:30:06

> If I want to run this on the A7 does that have any sort of CPU on which to
> run a typical program?

---

## 5 Jul 2026 04:19:27

> The spreadsheet is updated, re-read it.

---

## 5 Jul 2026 04:11:47

> Can the Tricorder be prototyped on this larger board without access to the
> A7?

---

## 5 Jul 2026 03:53:38

> Try again, I just copied it over.

---

## 5 Jul 2026 03:45:29

> The parts list is in docs now, go find it.

---

## 5 Jul 2026 03:41:44

> *(Pasted transcript of the claude.ai share link from the previous prompt —
> a separate Claude conversation titled roughly "FPGA as a sensor platform".
> Extracted content recorded in notebook entry 4. Transcript excerpts:)*
>
> Treat the FPGA as a sensor platform. What are good examples of data that
> needs low latency processing and to which an FPGA's customizable circuitry
> would be most useful.
>
> [Claude's answer: RF/SDR, network packet streams, machine vision,
> physics instrumentation, control loops]
>
> Are there already open source libraries of such circuits? Prioritize non
> network solutions for now. Physics and vision and real world sensors take
> priority.
>
> [Claude's answer: OHWR, hls4ml, FINN, Vitis Vision, PYNQ, Vitis DSP,
> LiteX/FuseSoC, ZipCPU cores]
>
> Provide a bibliography of papers that are published on these libraries or
> with them. Give links to papers and repositories in the vivl
>
> [Result: 14-entry BibTeX bibliography ("Fpga sensor libs" BIB artifact,
> not included in the paste); also a "Tricorder parts" XLSX with
> Adafruit-prioritized sensor parts list, not included in the paste]

---

## 5 Jul 2026 03:38:27

> Look at this link, can you extract the information from it
> https://claude.ai/share/3ff13c9b-f27d-47c6-ba76-99bd91a29c7b

---

## 5 Jul 2026 00:12:20

> Fix the commits with the rigth email.

---

## 5 Jul 2026 00:08:03

> I will push, you are NEVER to push

---

## 5 Jul 2026 00:07:26

> Add all files and commit

---

## 4 Jul 2026 07:34:03

> Study the open hardware site (ohwr.org) and find software moduels that
> coiuld run on the hardware in this host. The same requirements apply, the
> modules must be able to run in the dynamic region.

---

## 4 Jul 2026 07:26:08

> Look at the https://github.com/xilinx/vitis_libraries repo and explain
> wehther or not those libraries can be used in the dynamic region of the
> FPGA in this system.

---

## 4 Jul 2026 07:20:01

> This is a new repository for experiments having to do with creating dynamic
> sensor FGPA programs (each sensor type can be loaded or removed at runtime
> from the FGPA dynamic region). Create a docs directory with a notebook.md
> file useing the /notebook skill. Entries are kept in reverse chronological
> order and have both a date and a time (hours:min:sec) for when the entry was
> created. Also create a prompts.md in docs and record all prompts.
