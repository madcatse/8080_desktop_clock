# i8080 homebrew digital clock/weather station (fork)

## Русский

Этот репозиторий — форк проекта **"i8080 homebrew digital clock/weather station"** автора **Tronix286**: <https://github.com/Tronix286/i8080_clock>.

Моя цель — сделать часы более презентабельными и свести к минимуму использование SMD‑компонентов (с таким процессором использовать их «не комильфо»). Я очень благодарен автору за схему и само устройство — без этой базы не получилось бы так быстро продвинуться.

Ключевое отличие от исходного проекта: в этом форке сделан упор на внешний вид и на максимально сквозной/выводной монтаж, поэтому плата и компоновка ориентированы на более «классическое» исполнение и удобство сборки, при сохранении общей архитектуры.

### Что внутри

**Аппаратная часть**
- Плата i8080‑совместимого компьютера с тактированием, шиной, ПЗУ/ОЗУ и часами реального времени.
- Внешние устройства: TFT‑дисплей, датчики по I2C и 1‑Wire, кнопки.

**Проект в KiCad**
- Полный проект схемы и платы находится в каталоге `hardware/`.

**Фото**

![Плата в сборе](pics/photo_2023-10-27_10-43-03.jpg)
![Подключение внешних устройств](pics/photo_2023-10-27_10-47-33.jpg)

### ПО

Сборка прошивки описана в каталоге `software/` (используется z88dk и собственная цель `clock80`).

---

## English

This repository is a fork of **"i8080 homebrew digital clock/weather station"** by **Tronix286**: <https://github.com/Tronix286/i8080_clock>.

My goal is to make the clock look more presentable and to minimize the number of SMD parts (with a CPU like this, using SMD feels out of place). I am very grateful to the author for the schematic and the original device — it provided the essential foundation for this work.

Key difference vs the original project: this fork focuses on the physical appearance and on keeping the design as through‑hole as possible. The PCB layout and mechanical decisions are tailored to a more classic, build‑friendly presentation while preserving the overall architecture.

### What’s inside

**Hardware**
- i8080‑compatible system board with clocking, bus, ROM/RAM, and real‑time clock.
- External devices: TFT display, I2C and 1‑Wire sensors, and buttons.

**KiCad project**
- Full schematic and PCB project live under `hardware/`.

**Photos**

![Assembled board](pics/photo_2023-10-27_10-43-03.jpg)
![External devices](pics/photo_2023-10-27_10-47-33.jpg)

### Software

Firmware build instructions and sources are in `software/` (z88dk with a custom `clock80` target).
