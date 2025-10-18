# 📱 Smart Switch App - APK Installation Guide

## 🎉 **Your APK is Ready!**

**File Location:** `/Users/balaswamireddy/Documents/Project_supabase/SmartSwitchApp_v1.0.0.apk`
**File Size:** 54.7 MB
**App Version:** 1.0.0
**Package Name:** com.balaswami.smartswitch

---

## 🔧 **Installation Steps**

### **Step 1: Transfer APK to Your Android Phone**

**Option A: USB Cable**
1. Connect your Android phone to Mac via USB cable
2. Copy `SmartSwitchApp_v1.0.0.apk` to your phone's Downloads folder
3. Safely eject your phone

**Option B: Cloud Storage (Recommended)**
1. Upload APK to Google Drive, Dropbox, or any cloud service
2. Download on your Android phone
3. APK will be saved in Downloads folder

**Option C: AirDrop (if supported)**
1. Use AirDrop to send APK to your Android device
2. Save the file when received

### **Step 2: Enable Unknown Sources**

1. **Go to Settings** on your Android phone
2. Navigate to **Security** or **Privacy & Security**
3. Find **Install unknown apps** or **Unknown sources**
4. Select your **File Manager** app (usually "Files" or "Downloads")
5. **Enable** "Allow from this source"

### **Step 3: Install the APK**

1. Open **File Manager** on your Android phone
2. Navigate to **Downloads** folder
3. Tap on `SmartSwitchApp_v1.0.0.apk`
4. Tap **Install** when prompted
5. Wait for installation to complete
6. Tap **Open** or find the app in your app drawer

---

## 🏠 **App Features**

✅ **Remote Switch Control** - Control your ESP32 switches from anywhere
✅ **Real-time Updates** - See switch status changes instantly
✅ **Multiple Boards** - Manage multiple ESP32 boards
✅ **Custom Rooms** - Organize switches by rooms
✅ **Beautiful UI** - Modern glassmorphism design
✅ **Weather Integration** - Local weather information
✅ **Timer Functions** - Schedule switch operations

---

## 🔧 **Setup Instructions**

### **1. First Launch**
- App will ask for location permissions (for weather)
- Grant permissions for full functionality

### **2. Add Your ESP32 Board**
- Tap **"+"** to add a new board
- Enter your ESP32's Board ID (e.g., "BOARD_005")
- The app will automatically detect and claim your board

### **3. Control Your Switches**
- Tap any switch to toggle ON/OFF
- Changes sync with your physical ESP32 in real-time
- Physical button presses also update the app instantly

---

## 🐛 **Troubleshooting**

### **Installation Failed**
- **Check storage space** - App needs ~55MB free space
- **Re-enable unknown sources** for your file manager
- **Try different file manager** app if installation fails

### **App Crashes on Launch**
- **Restart phone** and try again
- **Clear cache** of recently installed apps
- **Ensure Android version** is 7.0+ (API level 24+)

### **Can't Connect to Switches**
- **Check WiFi connection** - App needs internet access
- **Verify ESP32 is online** and connected to same network
- **Check Board ID** matches your ESP32 configuration

### **Switches Not Responding**
- **Check ESP32 power** - Ensure ESP32 is powered on
- **Verify Supabase connection** - ESP32 needs internet access
- **Check physical wiring** - Relays and buttons properly connected

---

## 📊 **App Information**

- **Minimum Android Version:** Android 7.0 (API 24)
- **Target Android Version:** Android 15 (API 36)
- **Required Permissions:**
  - Internet access (for Supabase connection)
  - Location access (for weather features)
  - Storage access (for app data)

---

## 🔒 **Security Notes**

- App uses **Supabase authentication** for secure data access
- All switch commands are **encrypted** during transmission
- **Row Level Security** prevents unauthorized access to your boards
- APK is signed with **debug keys** (safe for personal use)

---

## 🚀 **Next Steps**

1. **Install APK** on your Android phone
2. **Test switch control** with your ESP32
3. **Add multiple rooms** and organize switches
4. **Set up timers** for automated control
5. **Enjoy your smart home system!**

---

## 📞 **Support**

If you encounter any issues:
1. Check ESP32 serial monitor for connection logs
2. Verify Supabase database connectivity
3. Ensure all switches are properly configured in database
4. Test physical button functionality on ESP32

**Happy Smart Home Controlling!** 🏠✨

---

## 🎯 **File Locations**

- **APK File:** `/Users/balaswamireddy/Documents/Project_supabase/SmartSwitchApp_v1.0.0.apk`
- **Source Code:** `/Users/balaswamireddy/Documents/Project_supabase/First_app/`
- **ESP32 Code:** `/Users/balaswamireddy/Documents/Project_supabase/First_app/ESP32_Your_Database.ino`