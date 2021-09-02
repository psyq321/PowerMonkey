# Reduce Notebook Thermal Waste and Gain Performance. Greener and Meaner!

```txt
   ______                            ______                 _
  (_____ \                          |  ___ \               | |
   _____) )___   _ _ _   ____   ___ | | _ | |  ___   ____  | |  _  ____  _   _
  |  ____// _ \ | | | | / _  ) / __)| || || | / _ \ |  _ \ | | / )/ _  )| | | |
  | |    | |_| || | | |( (/ / | |   | || || || |_| || | | || |< (( (/ / | |_| |
  |_|     \___/  \____| \____)|_|   |_||_||_| \___/ |_| |_||_| \_)\____) \__  |
                                                                        (____/
  Copyright (C) 2021 Ivan Dimkovic. All rights reserved.
  
  WARNING: This code is a proof of concept for educative purposes. It can
  modify internal computer configuration parameters and cause malfunctions or
  even permanent damage. It has been tested on a limited range of target CPUs
  and has minimal built-in failsafe mechanisms, thus making it unsuitable for
  recommended use by users not skilled in the art. Use it at your own risk.
```

- [What is PowerMonkey?](#what-is-powermonkey)
  - [Background](#background)
  - [Challenges](#challenges)
  - [How does PowerMonkey work?](#how-does-powermonkey-work)
- [Enabled Features](#enabled-features)
- [Getting Started](#getting-started)
- [Build the binaries](#build-the-binaries)
  - [Building - "Visual Studio" route](#building---visual-studio-route)
  - [Building - EDK2 route](#building---edk2-route)
- [Testing](#testing)
  
## What is PowerMonkey?

### **Background**

By the 2020s, everybody knows what ‘overclocking’ is: bringing the system to its limits by increasing its working frequency, voltage or both. This process used to be very error-prone but it got perfected in the meantime, with even some big-name systems coming “factory overclocked” out of the box.

However, much fewer people have heard about its cousin: **undervolting**. Undervolting is, in a way, an opposite process from overclocking: reducing CPU voltage to the point, CPU consumes much less power and generates way less heat.

Intriguingly, doing this to the thermally limited system such as a notebook or SFF (small form factor) PC will **both reduce heat AND improve performance!** Magic? No, physics! There is only so much heat a notebook cooling system can evacuate, and once this route becomes exhausted, there is no other option but to throttle the CPU to prevent catastrophic failure. Reducing voltage at high frequencies significantly decreases power and heat output thus making the system cooler **and** faster.

### **Challenges**

Sadly, it has become increasingly difficult to optimize a typical notebook. First, undervolting became mostly locked-by-default by OEMs. This problem can be mitigated by “unlocking” the firmware settings so that excellent applications like [ThrottleStop](https://www.techpowerup.com/download/techpowerup-throttlestop/) can do their job (unless the platform is fully locked).

But new problems are in sight: Hypervisors such as Microsoft Hyper-V are now used even by end-user applications like WSL2 (Windows Subsystem for Linux). Because Hypervisor does not allow a Virtual Machine to modify most of the host hardware registers, applications like ThrottleStop or Intel’s XTU cannot adjust voltages.

When application attempts to write to a hardware register, Hypervisor traps this attempt and does not perform the actual writing. It is  heoretically possible that Hypervisor vendor implements writing, but highly unlikely. This has created new barriers for undervolting and similar types of hardware tweaks.

Nothing above would be a problem if OEMs allowed customers to adjust the settings in the firmware setup. However, most notebooks come with minimal configuration options, locking the owner of being able to adjust their hardware. PowerMonkey POC intends to fix precisely this problem, and this repository is the first “proof-of-concept” showing viability of such an approach.

### **How does PowerMonkey work?**

This proof-of-concept is using ability of UEFI firmwares to execute so-called "applications" before OS bootoader is invoked. At that moment (providing that the firmware did not lock) it is possible to (almost) fully adjust CPU frequency and voltages as well as power management configuration. This is later in the boot process (see below) as opposed to how OEM would do it, but since "advanced setup" is not an option, this is actually the second best place.

Below picture explains where PowerMonkey.efi POC resides:

![EfiGood](img/boot.svg)

## Enabled Features

NOTE: PowerMonkey is a POC and is **not tested** on anything other than few CML (10th Gen) systems. At this stage, usage is recommended only to developers and experts under precautions. There are no checks for CPU compatibility so crashes could occur. It can brick computers or cause them to fail in all kinds of ways!

| Feature                                                 |       Status       |
| ------------------------------------------------------- | ------------------ |
| **Voltage / Frequency Overrides**                       |                    |
|   Voltage Override for entire VF Curve                  |       Enabled      |
|   Targetted voltage mode                                |       Enabled      |
|   Interpolative voltage mode                            |       Enabled      |
|   Voltage offset                                        |       Enabled      |
|   Voltage adjustment on individual VF Points            |     In Progress    |
| **Turbo Bins**                                          |                    |
|   Max. Turbo Ratios for different core counts           |       Enabled      |
| **Power Limits**                                        |                    |
|   Enable/Disable/Lock PL1/PL2/PL3/PL4/PP0/...           |       Enabled      |
|   Power and Time Limits adjustment (where possible)     |       Enabled      |
|   Package and Platform (PSys) Domains                   |       Enabled      |
|   MSR and MMIO Control                                  |       Enabled      |
| **Lockdown**                                            |                    |
|   Optional Overclocking Lockdown                        |       Enabled      |
|   Fine-grained Lockdown of different Power registers    |       Enabled      |

## Getting Started

## Build the binaries

Because PowerMonkey is in the “proof-of-concept” stage right now. It is also necessary to adjust the CPU voltage/frequency/power override parameters to match your CPU. It is essential to build PowerMonkey from the source code.

1. Clone PowerMoneky repository (this repository)

2. Decide if you want to build `PowerMonkey.efi` using ‘official’ UEFI EDK (TianoCore), or if you wish to handle everything in Visual Studio?

After you have decided how to build PowerMonkey, please take some time to review and understand `CONFIGURATION.c` file where the entire configuration interface and options reside. Configuration process is exactly the same for both build methods and does not depend on the choice of build toolchain.

### **Building - "Visual Studio" route:**

This route is based on Alex Ionescu’s excellent [VisualUEFI project](https://github.com/ionescu007/VisualUefi). The advantage of this approach is that you do not need to deal with the EDK2 build system and its config files.

1. Fetch VisualUEFI from [https://github.com/ionescu007/VisualUefi](https://github.com/ionescu007/VisualUefi) and build its prerequisites (follow the instructions)

2. Store PowerMonkey directory (and its subdirectories) directly in the root of the VisualUEFI repo:

    ```bash
    VisualUEFI
    ├── debugger/
    ├── EDK-II/
    ├── edk2/
    ├── openssl/
    ├── PowerMonkey/                           <––– Our Workspace
    │   ├── PowerMonkeyApp/                    <--- Source code and configuration
    │   ├── LICENSE-2.0.txt
    │   ├── PmWorkspace.default.props
    │   ├── PmWorkspace.props
    │   ├── PmWorksspace.sln                   <--- Visual Studio Solution File
    │   └── README.MD
    ├── Samples/
    └── ...
    ```

3. Open ```PmWorkspace.sln``` and build the project directly in Visual Studio. This process shall generate no warnings or errors. The result should be an EFI executable ```/PowerMonkey/x64/Release/PowerMonkey.efi```

### **Building - EDK2 route:**

1. Download EDK2 source from [https://github.com/tianocore/edk2](https://github.com/tianocore/edk2) - ideally, the EDK2 version shall match your computer firmware “baseline EDK2 version” in case your UEFI firmware is based on TianoCore.

2. Store PowerMonkeyApp directory (and its subdirectories) in the ```/MdeModulePkg/Application``` subdirectory of EDK2 workspace. The result shall look something like this (there will be more directories):

    ```bash
    EDK
    ├── BaseTools/
    ├── Conf/
    ├── MdeModulePkg/
    │   ├── Application/
    │   │   ├── BootManagerMenuApp/
    │   │   ├── CapsuleApp/
    │   │   ├── PowerMonkeyApp/         <––– Our Project
    │   │   └── UiApp/
    │   ├── Include/
    │   └── Library/
    ├── UefiCpuPkg/
    ├── edksetup.bat
    └── ...
    ```

3. Edit the `/MdeModulePkg/MdeModulePkg.dsc` file, and add PowerMonkey project inside:

    ```ini
    [Components]
    MdeModulePkg/Application/PowerMonkeyApp/PowerMonkey.inf
    ```

4. Building process that follows is identical as for any other EDK2 project (edksetup.bat, build, ...).

## Testing

In order to prevent reboot-loops it is highly advisable to first test ```PowerMonkey.efi``` by loading it from EFI shell or from a separate Booltloader entry (such as GRUB2). This way it is easy to revert back to original settings.

```PowerMonkey.efi``` shall be executed before OS Bootloader. This can be achieved in many ways, the most obvious and least automated would be invocation through EFI shell. Executing ```PowerMonkey.efi``` shall result in something like this:

In case of failure, you will probably either see a crash information or a frozen computer. If everything worked, the view from the EFI shell should look like this:

![EfiGood](img/efigood.png)

If the computer is still alive and not frozen, you might proceed to your OS of choice to confirm that the settings have been applied. Do not forget to disable Hypervisor first, as one enabled, you will not be able to actually see the voltages from a VM. Settingfs form the ```CONFIGURATION.c``` applied to my CPU result in the following (pictures of ThrottleStop and XTU just reading the values from the CPU):

**Intel XTU:**

![EfiGood](img/powah_xtu1.jpg)

**ThrottleStop:**

![EfiGood](img/powah_ts1.png)

This confirms that our settings have been applied including the locks (we locked the configuration so that it cannot be changed until the next reset). Now we can re-enable the Hypervisor and continue testing.
