.. _memfault_sample:

nRF9160: Memfault
#################

.. contents::
   :local:
   :depth: 2

This sample shows how to use the `Memfault SDK`_ in an NCS application to collect coredumps and metrics.
The sample connects to LTE network and sends the collected data to Memfault's cloud using HTTPS.


Requirements
************

Before the Memfault platform can be used, an account must be registered on `Memfault`_ and a project must be set up according to the `Memfault Docs`_.

The application supports the following development kits:

.. table-from-rows:: /includes/sample_board_rows.txt
   :header: heading
   :rows: thingy91_nrf9160ns, nrf9160dk_nrf9160ns

.. include:: /includes/spm.txt


Overview
********

The Memfault sample demonstrates how the Memfault SDK can be used as a module in NCS to collect coredumps, reboot reasons, metrics and trace events from devices and send to the Memfault cloud.
For more details on the various Memfault concepts, please refer to `Memfault Terminology`_ .


Metrics
=======

The sample implements metrics both for system properties and custom to the application.
Details on how metrics work and can be implemented is found at `Memfault: Collecting Device Metrics`_.
Some metrics are collected by the Memfault SDK directly, and will not be described in greater detail here.
There are also some NCS-specific metrics that are enabled by default:

*  LTE metrics which can be enabled and disabled using :option:`CONFIG_MEMFAULT_NCS_LTE_METRICS`:

   *  ``lte_time_to_connect``: Tracking the time from the device starts to search for an LTE network until it is registered with the network.
   *  ``lte_connection_loss_count``: The number of times that the device has lost the LTE network connection after the initial network registration.

*  Stack usage metrics, showing how many bytes of unused space is left in a stack, configurable using :option:`CONFIG_MEMFAULT_NCS_STACK_METRICS`:

   *  ``at_cmd_unused_stack``: The AT command library's stack.
   *  ``connection_poll_unused_stack``: Stack used by the cloud libraries for nRF Cloud, AWS IoT and Azure IoT Hub.

In addition to the metrics provided by the Memfault SDK integration layer in NCS, the sample also shows how to capture an application-specific metric.
These metric is defined in :file:`samples/nrf9160/memfault/config/memfault_metrics_heartbeat_config.h`:

*  ``switch_1_toggle_count``: The number of times that switch 1 has been toggled.
  Switch 1 is available on nRF9160 DK.


Error Tracking with Trace Events
================================

The sample implements a user-defined trace reason for demonstration purposes.
The trace reason is called ``switch_2_toggled``, and is collected every time switch 2 is toggled.
Along with tracing that the event occured, the trace includes the current switch state.
More information on how to configure and use trace events is found at `Memfault: Error Tracking with Trace Events`_.


Coredumps
=========

Coredumps can be triggered either by using the Memfault shell command ``mflt crash``, or by

will for demonstration purposes trigger faults when buttons are pressed:

*  Button 1 triggers a stack overflow
*  Button 2 triggers a NULL-pointer dereference

These faults will cause crashes that will be captured by Memfault.
After rebooting, the crash data can be sent to the Memfault cloud for further inspection and analysis.
For more information on the debugging possibilities that the Memfault platform offers, see `Memfault Docs`_.

The sample enables Memfault shell by default.
The shell offers multiple test commands to test a wider range of functionality offered by Memfault SDK.
For more information on the available commands, send the command `mflt help` in the terminal.


Configuration
*************

The Memfault SDK exposes some of its configuration options in Kconfig.
For those options in the SDK that are not available for configuration using Kconfig, it's possible to use :file:`samples/nrf9160/memfault/config/memfault_platform_config.h` as well.
Please refer to `Memfault SDK`_ for more information.
The rest of this section will focus on Kconfig options.


Minimal setup
=============

To be able to send data to the Memfault cloud, a project key must be configured using :option:`CONFIG_MEMFAULT_PROJECT_KEY`.

.. note::
   The Memfault SDK requires ertificates required for the HTTPS transport.
   The certificates are by default provisioned automatically by the NCS integration layer for Memfault SDK to sec tags 1001 - 1005.
   If other certificates exist at these sec tags, HTTPS uploads will fail.


Configuration options
=====================

There are two sources for Kconfig options when using Memfault SDK in NCS: Kconfig options defined within the Memfault SDK and Kconfig options defined in the NCS integration of the Memfault SDK.
The latter ones are prefixed ``CONFIG_MEMFAULT_NCS``.

Memfault SDK options:

.. option:: CONFIG_MEMFAULT - Enable Memfault SDK

   This option is needed to be able to compile code that uses Memfault SDK APIs.

.. option:: CONFIG_MEMFAULT_ROOT_CERT_STORAGE_NRF9160_MODEM - Provision certificates to nRF9160

   This option is enabled by default when using nRF9160 DK or Thingy:91.
   It will make sure that :c:func:`memfault_zephyr_port_install_root_certs` provisions the certificates to the modem when it's called.

.. option:: CONFIG_MEMFAULT_SHELL - Enable Memfault shell

.. option:: CONFIG_MEMFAULT_HTTP_ENABLE - Use HTTP as transport for data to Memfault cloud

.. option:: CONFIG_MEMFAULT_HTTP_PERIODIC_UPLOAD - Enable periodic HTTP uploads

.. option:: CONFIG_MEMFAULT_HTTP_PERIODIC_UPLOAD_INTERVAL_SECS - Set the periodic HTTP upload interval

.. option:: CONFIG_MEMFAULT_COREDUMP_COLLECT_BSS_REGIONS - Collect bss region as part of the coredump data.


NCS-specific Memfault options:

.. option:: CONFIG_MEMFAULT_NCS_PROJECT_KEY - Set the Memfault project key

.. option:: CONFIG_MEMFAULT_NCS_LTE_METRICS - Collect LTE metrics

   Collect LTE metrics when the application is running.
   The supported LTE metrics are time to LTE connection is established and number of LTE connection losses.

.. option:: CONFIG_MEMFAULT_NCS_STACK_METRICS - Collect stack metrics

   Collect stack usage metrics from AT command library stack and nRF Cloud, AWS IoT and Azure IoT Hub stacks.

.. option:: CONFIG_MEMFAULT_NCS_INTERNAL_FLASH_BACKED_COREDUMP - Store coredumps to internal flash

   Storing coredumps to internal flash will let the data survive power loss.


Building and running
********************

.. |sample path| replace:: :file:`samples/nrf9160/memfault`
.. include:: /includes/build_and_run.txt
.. include:: /includes/spm.txt

Testing
=======

Before testing, ensure that your device is configured with your Memfault projects's project key.
After programming the sample to your device, test it by performing the following steps:

1. Connect the USB cable and power on or reset your device.
#. Open a terminal emulator and observe that the sample has started.
   The output to the terminal should look similar to the one below:

   .. code-block:: console

        *** Booting Zephyr OS build v2.4.99-ncs1-4934-g6d2b8c7b17aa  ***
        <inf> <mflt>: Reset Reason, RESETREAS=0x0
        <inf> <mflt>: Reset Causes:
        <inf> <mflt>:  Power on Reset
        <inf> <mflt>: GNU Build ID: a09094cdf9da13f20719f87016663ab529b71267
        <inf> memfault_sample: Memfault sample has started

#. The sample will now connect to an available LTE network, indicated by

   .. code-block:: console

        <inf> memfault_sample: Connecting to LTE network, this may take several minutes...

#. When the connection is established, the sample will proceed to display the captured LTE time-to-connect metric:

   .. code-block:: console

        <inf> memfault_sample: Connected to LTE network. Time to connect: 3602 ms

#. Subsequently, all captured Memfault data will be sent to the Memfault cloud:

   .. code-block:: console

        <inf> memfault_sample: Sending already captured data to Memfault
        <dbg> <mflt>: Response Complete: Parse Status 0 HTTP Status 202!
        <dbg> <mflt>: Body: Accepted
        <dbg> <mflt>: Response Complete: Parse Status 0 HTTP Status 202!
        <dbg> <mflt>: Body: Accepted
        <dbg> <mflt>: No more data to send

#. Press <TAB> on your keyboard to confirm that the Memfault shell is working.
   The available shell commands should now be displayed:

   .. code-block:: console

        uart:~$
          clear              device             flash              help
          history            kernel             log                mcuboot
          mflt               mflt_nrf           nrf_clock_control  resize
          shell

#. Explore the available Memfault shell commands by issuing the command ``mflt help``.

#. Press button 1 or 2 to trigger a stack overflow or a NULL-pointer dereference, respectively.

#. In a web browser, navigate to `Memfault`_, sign in and follow the `Memfault Docs`_ on viewing the uploaded data.
   Note that the symbol file for the sample must be uploaded in order for Memfault to parse the information.
   The symbol file is located at the following path: ``<sample folder>/build/zephyr/zephyr.elf``.


Dependencies
************

The sample requires the Memfault SDK, which is part of NCS's west manifest, and will be downloaded automatically when running ``west update``.

This sample uses the following |NCS| libraries and drivers:

* :ref:`dk_buttons_and_leds_readme`
* :ref:`lte_lc_readme`

It uses the following `sdk-nrfxlib`_ library:

* :ref:`nrfxlib:nrf_modem`

In addition, it uses the following sample:

* :ref:`secure_partition_manager`
