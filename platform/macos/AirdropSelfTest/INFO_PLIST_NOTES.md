Add the following key to `Info.plist` (or **Target → Info** in Xcode). Without it, macOS will not allow the application to request Bluetooth access, and the app may either fail to discover devices or crash when CoreBluetooth is first initialized.

**Key:** `Privacy - Bluetooth Always Usage Description` (`NSBluetoothAlwaysUsageDescription`)

**Value:**

```text
The application requires Bluetooth access to transfer files between devices without Wi-Fi.
```

Also, under **Target → Signing & Capabilities**, add the **App Sandbox** capability (or disable the sandbox during development).

If App Sandbox is enabled, make sure Bluetooth access is enabled in its capabilities/entitlements. Otherwise, CoreBluetooth may not be able to access the Bluetooth hardware.
