.. _lib_nrf_cloud_pgps:

nRF Cloud P-GPS
###############

.. contents::
   :local:
   :depth: 2

The nRF Cloud P-GPS library enables applications to request and process predicted GPS data from `nRF Connect for Cloud`_ to be used with the nRF9160 SiP.
This library is an enhancement to the :ref:`lib_nrf_cloud`. It may be used with or without Assisted GPS (`A-GPS`_) data from `nRF Connect for Cloud`_.

To get a position fix, a GPS receiver needs information such as the satellite orbital data, exact time, and accurate clock frequency data.
GPS satellites broadcast this information in a pattern, which repeats every 12.5 minutes.
If nRF Connect for Cloud P-GPS and/or A-GPS service is used, the broadcasted information, and future estimates of it, can be downloaded at a faster rate from nRF Connect for Cloud.

Predicted GPS data contains estimated orbits (ephemerides) of the 32 GPS satellites for up to a two week period, with each set of ephemerides predictions being valid for a specific four hour period within the set of all predictions provided.
These ephemeris predictions are downloaded from the cloud, then stored by the device in Flash memory, then later injected into the GPS unit when needed.

The use of P-GPS reduces the time for a GPS device to estimate its position, which is also called Time to First Fix (TTFF), compared to using no assistance at all.
Further, it only requires a cloud connection once a week or so, depending on configuration, rather than each time a fix is needed, when A-GPS alone is used.
When used together with A-GPS, TTFF is even faster than with just P-GPS, while the amount of cloud data needed for each fix is smaller during each fix than with A-GPS alone.
With proper configuration, A-GPS can be used with P-GPS only when a cloud connection is available, but will get fast fixes even without a connection.
This will be the case as long as stored P-GPS data is still valid, and current time within a few seconds and most recent location within a few km are known.

.. note::
   To use the nRF Connect for Cloud P-GPS service, an nRF Connect for Cloud account is needed, and the device needs to be associated with a user's account.

Configuration
*************

Configure the following options to enable or disable the use of this library:

* :option:`CONFIG_NRF_CLOUD`
* :option:`CONFIG_NRF_CLOUD_PGPS`

Configure these additional options to refine the behavior of P-GPS:

* :option:`CONFIG_NRF_CLOUD_PGPS_PREDICTION_PERIOD`
* :option:`CONFIG_NRF_CLOUD_PGPS_NUM_PREDICTIONS`
* :option:`CONFIG_NRF_CLOUD_PGPS_REPLACEMENT_THRESHOLD`
* :option:`CONFIG_NRF_CLOUD_PGPS_DOWNLOAD_FRAGMENT_SIZE`

Configure both of the following options if you would like your application to use A-GPS as well, for coarse time and position data, to get the fastest TTFF.
If A-GPS is not desired (due to data costs, low power requirements, or expected frequent lack of lack of cloud connectivity), both should be disabled.

* :option:`CONFIG_NRF_CLOUD_AGPS`
* :option:`CONFIG_AGPS`

Within an application that uses P-GPS, these options are needed, for storing settings, having accurate clock time, and having a location to store predictions in:

* :option:`CONFIG_FLASH`
* :option:`CONFIG_FCB`
* :option:`CONFIG_SETTINGS_FCB`
* :option:`CONFIG_DATE_TIME`
* :option:`CONFIG_BOOTLOADER_MCUBOOT`
* :option:`CONFIG_IMG_MANAGER`
* :option:`CONFIG_MCUBOOT_IMG_MANAGER`

See :ref:`configure_application` for information on how to change configuration options.

Initialization
**************

The P-GPS subsystem's :c:func:`nrf_cloud_pgps_init` function takes a pointer to a :c:struct:`nrf_cloud_pgps_init_param` which at a minimum must specify the storage base address and storage size, in Flash, to which the P-GPS subsystem can store predictions.
It can optionally pass a pointer to a :c:func:`pgps_event_handler_t` callback function.

The sample :ref:`agps_sample` shows how to pass the address of the secondary MCUBOOT :ref:`mcuboot_ncs` partition whose address is defined by the :c:macro:`PM_MCUBOOT_SECONDARY_ADDRESS` macro and the :c:macro:`PM_MCUBOOT_SECONDARY_SIZE` macro.
These are automatically defined by the build system in the file pm_config.h. This partition is safe to store data in until a FOTA job is received.

Ideally, the application should call the P-GPS initialization function once the device has connected to the cloud.
If a connection is not available, initialization should still be called, but predictions will be unavailable if all valid predictions are expired, until connected to the cloud in the future.

.. note::
   Each prediction requires 2KiB of Flash. For 240 minute (4 hour) prediction periods, and 42 predictions in one week, that adds up to 84KiB.

Time
****

The P-GPS subsystem's proper operation depends on an accurate sense of time. For use cases where a cloud connection occurs easily, it is recommended that the :ref:`lib_date_time` library be used with NTP enabled.
Otherwise, a battery-backed real time clock calendar chip should be used so that accurate time is available regardless of cloud availability after reset.

Requesting and processing P-GPS data
************************************

P-GPS data can be requested from the cloud using one of the following methods:

* Directly, by calling the function :c:func:`nrf_cloud_pgps_request_all` to request a full set of predictions
* Directly, by passing a properly initialized :c:struct:`gps_pgps_request` structure to the :c:func:`nrf_cloud_pgps_request` function
* Indirectly, by calling :c:func:`nrf_cloud_pgps_init`, and no valid predictions are present in Flash, or some or all are expired
* Indirectly, by calling :c:func:`nrf_cloud_pgps_preemptive_updates`
* Indirectly, by calling :c:func:`nrf_cloud_pgps_notify_prediction`

The indirect methods are used in the sample :ref:`agps_sample` and in :ref:`asset_tracker`.

When nRF Connect for Cloud responds with the requested P-GPS data, the :c:func:`nrf_cloud_pgps_process` function should be called by the application's :c:func:`cloud_evt_handler_t` when it receives the :c:enum:`CLOUD_EVT_DATA_RECEIVED` event.
The function parses the data and stores it.

Finding a prediction and injecting to modem
*******************************************

A P-GPS prediction for the current date and time can be retrieved using one of the following methods:

* Directly, by calling the function :c:func:`nrf_cloud_pgps_find_prediction`
* Indirectly, by calling the function :c:func:`nrf_cloud_pgps_notify_prediction`

The indirect method is used in the sample :ref:`agps_sample` and in :ref:`asset_tracker`.

The application can inject the data contained in the prediction to the GPS unit in the modem by calling the :c:func:`nrf_cloud_pgps_inject` function.

Interaction with the GPS driver
*******************************

The P-GPS subsystem, like a number of other nRF Connect for Cloud subsystems, is event driven.

There are two GPS events that an application receives through the GPS driver callback which relate to P-GPS.  One, :c:enum:`GPS_EVT_AGPS_DATA_NEEDED`, occurs when the GPS module requires assistance data.
Another, :c:enum:`GPS_EVT_PVT_FIX` occurs once a fix has been attained.

When the application receives the first event, it should call :c:func:`nrf_cloud_pgps_notify_prediction`.
This will call back the application's :c:func:`pgps_event_handler_t` when a valid P-GPS prediction set is available, by passing the handler the :c:enum:`PGPS_EVT_AVAILABLE` event and a pointer to a :c:struct:`nrf_cloud_pgps_prediction`.

The application should pass this prediction to :c:func:`nrf_cloud_pgps_inject`, along with the :c:struct:`gps_agps_request` passed to the GPS driver callback earlier with the :c:enum:`GPS_EVT_AGPS_DATA_NEEDED` event.

If the use case for the application is such that motion of the device will be slow, when it receives the other GPS event, :c:enum:`GPS_EVT_PVT_FIX`, it should pass the resulting latitude and longitude to :c:func:`nrf_cloud_pgps_set_location`.
The P-GPS subsystem will use that stored location the next time the A-GPS request asks for position assistance, and A-GPS assistance is not enabled or is unavailable.
If the use case involves rapid motion at times and/or infrequent GPS fix attempts, such a mechanism could be detrimental to short TTFF, as the saved position may be too inaccurate to be a benefit.

The application could also call :c:func:`nrf_cloud_pgps_preemptive_updates` to discard expired predictions and replace them with newer ones, prior to the entire set expiring.
This can be useful for customer use cases where clould connections are only available infrequently.
The :option:`CONFIG_NRF_CLOUD_PGPS_REPLACEMENT_THRESHOLD` sets the minimum number of valid predictions remaining before such an update occurs, and must even.

For best performance, it is helpful for applications to call the P-GPS functions mentioned in this section from workqueue handlers.

The P-GPS subsystem itself generates events which can be passed to a registered callback function. See :c:enum:`nrf_cloud_pgps_event`.

API documentation
*****************

| Header file: :file:`include/net/nrf_cloud_pgps.h`
| Source files: :file:`subsys/net/lib/nrf_cloud/src/`

.. doxygengroup:: nrf_cloud_pgps
   :project: nrf
   :members:
