# AuroraOS Custom Commands

AuroraOS v17 adds a simple user-space command system.

## Idea
You do **not** need to change the kernel to add a new command.

A custom command consists of:
1. a **script file** somewhere in AuroraFS
2. a small **wrapper file** in `~/bin`

The wrapper stores the path to the script file.
The wrapper name becomes the command name.

## Quick start
Create a starter command:

```text
cmd new hallo
```

This creates:
- `~/hallo.aos`
- `~/bin/hallo.cmd`

Then run it by typing:

```text
hallo
```

## Register an existing script
If you already created a script file, register it like this:

```text
cmd add backup ~/backup.aos
```

Now you can run it with:

```text
backup
```

## Manage commands
```text
cmd list
cmd show backup
cmd remove backup
```

## Script format
Scripts are plain text files.
Each line is just a normal AuroraOS shell command.

Example:

```text
echo Hallo
pwd
ls
```

## Arguments
Simple argument expansion is supported:
- `$@` = all arguments
- `$1` .. `$9` = first to ninth argument

Example script:

```text
echo Hallo $1
```

Then:

```text
hallo Welt
```

## Current limitation
AuroraOS does **not** run Python directly yet.
So the wrapper idea is similar to Linux, but the executable script content is currently **AuroraOS shell script**, not real Python.
