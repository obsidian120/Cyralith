# AuroraOS external programs and security

AuroraOS v18 introduces a first safe external program model.

## Idea
A program is no longer just “some script file”.
It now has:
- a command wrapper in `/apps/commands`
- a manifest in `/apps/programs/<name>.app`
- an entry script, usually `~/name.aos`

## Manifest fields
- `entry=` script path
- `trust=` trust level such as `local` or `trusted`
- `caps=` capabilities like `fs-read`, `fs-write`, `network`, `apps`, `system`
- `approved=` yes/no
- `owner=` user who owns the program

## Default behavior
When you create a command with:
- `cmd new hello`
- `cmd add backup ~/backup.aos`

AuroraOS automatically creates a manifest with:
- trust: `local`
- caps: `fs-read,fs-write`
- approved: `yes`

## Safety rules
Scripts are sandboxed.
By default they may:
- read files
- write only in the current user's home or in `/temp`

They may not automatically:
- elevate privileges
- change system networking
- reboot the OS
- install/remove apps
- change file ownership/rights

Those actions need stronger capabilities and, in some cases, system mode.

## Useful commands
- `prog list`
- `prog info <name>`
- `prog caps <name> fs-read,fs-write`
- `prog trust <name> local`
- `prog approve <name>`
- `prog run <name>`
- `which <name>`
