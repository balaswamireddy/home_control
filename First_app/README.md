# Smart Home IoT Controller

A comprehensive Flutter application for controlling ESP32-based smart home devices using **Supabase's hybrid SQL + Realtime architecture**.

## 🏠 Project Overview

This project evolved from a Firebase-based smart home system to a modern **Supabase hybrid architecture** that combines:

- **SQL Tables**: For complex data relationships, user management, and data integrity
- **Realtime Database**: For instant ESP32 ↔ Flutter synchronization
- **ESP32 Integration**: Physical switch control with WiFi connectivity
- **Multi-platform Support**: Android, iOS, and Web applications

## 🚀 Key Features

### 📱 Flutter App Features
- **Real-time Switch Control**: Instant on/off toggle with visual feedback
- **Multi-Home Management**: Support for multiple homes and rooms
- **User Authentication**: Secure login with Supabase Auth
- **Device Discovery**: Automatic ESP32 board detection
- **Home Sharing**: Share access with family members
- **Timer Scheduling**: Automated switch control
- **Alexa Integration**: Voice control support
- **Custom Themes**: Dark/light mode with custom wallpapers

### 🔧 ESP32 Features
- **WiFi Configuration Portal**: Easy setup via captive portal
- **Physical Switch Control**: Hardware button integration
- **Real-time Sync**: Instant state synchronization with app
- **Auto-reconnection**: Robust connection handling
- **OTA Updates**: Over-the-air firmware updates
- **Status Monitoring**: Online/offline detection
- **Multi-switch Support**: Up to 4 switches per board

## 🏗️ Architecture

### Hybrid Database Design
```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   ESP32 Device  │    │   Flutter App   │    │   Supabase DB   │
│                 │    │                 │    │                 │
│  ┌───────────┐  │    │  ┌───────────┐  │    │  SQL Tables:    │
│  │ Physical  │  │    │  │ UI Widgets│  │    │  • homes        │
│  │ Switches  │  │    │  │ Controls  │  │    │  • rooms        │
│  └───────────┘  │    │  └───────────┘  │    │  • boards       │
│        │         │    │        │        │    │  • switches     │
│  ┌───────────┐  │    │  ┌───────────┐  │    │  • users        │
│  │ WiFi +    │◄─┼────┼─►│ Realtime  │◄─┼────┤  • timers       │
│  │ HTTP API  │  │    │  │ Service   │  │    │                 │
│  └───────────┘  │    │  └───────────┘  │    │  Realtime:      │
│        │         │    │        │        │    │  • realtime_sw  │
│  ┌───────────┐  │    │  ┌───────────┐  │    │  • device_status│
│  │ Auto-sync │  │    │  │ Stream    │  │    │                 │
│  │ Background│  │    │  │ Updates   │  │    │                 │
│  └───────────┘  │    │  └───────────┘  │    └─────────────────┘
└─────────────────┘    └─────────────────┘
```

For detailed architecture information, see [SUPABASE_REALTIME_ARCHITECTURE.md](SUPABASE_REALTIME_ARCHITECTURE.md).

## 📋 Prerequisites

### Development Environment
- **Flutter SDK**: 3.10.0 or higher
- **Dart SDK**: 3.0.0 or higher
- **Android Studio** or **VS Code** with Flutter extensions
- **Git** for version control

### Hardware Requirements
- **ESP32 Development Board** (ESP32-WROOM-32 recommended)
- **Relay Modules** (4-channel relay board)
- **Push Buttons** for physical control
- **LEDs** for status indication
- **Breadboard and Jumper Wires**

### Services
- **Supabase Account**: [https://supabase.com](https://supabase.com)
- **Android/iOS Development Setup**

## 🛠️ Installation & Setup

### 1. Clone the Repository
```bash
git clone <repository-url>
cd First_app
```

### 2. Install Flutter Dependencies
```bash
flutter pub get
```

### 3. Supabase Setup

#### Create Supabase Project
1. Go to [https://supabase.com](https://supabase.com)
2. Create a new project
3. Note your **Project URL** and **anon key**

#### Database Setup
1. Go to Supabase Dashboard → SQL Editor
2. Copy the contents of `database/COMPLETE_APP_SCHEMA.sql`
3. Paste and run the SQL script
4. Verify tables are created successfully

#### Configure Environment
Create `lib/config/supabase_config.dart`:
```dart
class SupabaseConfig {
  static const String url = 'YOUR_SUPABASE_URL';
  static const String anonKey = 'YOUR_SUPABASE_ANON_KEY';
}
```

### 4. ESP32 Setup

#### Hardware Connections
```
ESP32 Pin → Component
GPIO 13   → Relay 1 IN
GPIO 12   → Relay 2 IN
GPIO 14   → Relay 3 IN
GPIO 27   → Relay 4 IN

GPIO 26   → Button 1
GPIO 25   → Button 2
GPIO 33   → Button 3
GPIO 32   → Button 4

GPIO 2    → Status LED
3.3V      → VCC (Relays, Buttons)
GND       → GND (Relays, Buttons)
```

#### Firmware Upload
1. Install **Arduino IDE** or **PlatformIO**
2. Install ESP32 board support
3. Open `lib/resources/supabase_esp32.c`
4. Update Supabase URL and key in the code
5. Upload to ESP32

### 5. Flutter Configuration

#### Update Supabase Dependencies
Ensure these dependencies are in `pubspec.yaml`:
```yaml
dependencies:
  flutter:
    sdk: flutter
  supabase_flutter: ^1.10.0
  # ... other dependencies
```

#### Initialize Supabase in Main
```dart
import 'package:supabase_flutter/supabase_flutter.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  
  await Supabase.initialize(
    url: SupabaseConfig.url,
    anonKey: SupabaseConfig.anonKey,
  );
  
  runApp(MyApp());
}
```

## 🚀 Running the Application

### Flutter App
```bash
# Run on Android/iOS
flutter run

# Run on Web
flutter run -d chrome

# Build APK
flutter build apk --release
```

### ESP32 Device
1. **First-time Setup**:
   - Power on ESP32
   - Press Switch 1 seven times quickly
   - Connect to WiFi "SmartSwitch_BOARD_XXX"
   - Configure WiFi credentials via web portal

2. **Normal Operation**:
   - Device auto-connects to WiFi
   - Syncs with Supabase database
   - Physical buttons control relays
   - App shows real-time status

## 💡 Usage Guide

### Adding a New Device
1. Open Flutter app
2. Go to "Add Device" screen
3. Enter board ID (e.g., "BOARD_001")
4. Assign to home and room
5. Device appears in app once online

### Controlling Switches
- **Physical**: Press buttons on ESP32
- **App**: Tap switch widgets
- **Voice**: "Alexa, turn on living room light"
- **Timer**: Schedule automatic on/off

### Managing Homes & Rooms
1. Create homes in the app
2. Add rooms to organize devices
3. Share homes with family members
4. Set permissions for each user

## 🔧 Development

### Project Structure
```
lib/
├── main.dart                 # App entry point
├── models/                   # Data models
├── services/                 # Business logic
│   ├── supabase_realtime_service.dart
│   ├── auth_service.dart
│   └── board_service.dart
├── widgets/                  # Reusable UI components
│   └── realtime_switch_widget.dart
├── screens/                  # App screens
├── providers/                # State management
└── utils/                    # Helper functions

database/
└── COMPLETE_APP_SCHEMA.sql   # Database schema

lib/resources/
├── supabase_esp32.c          # New Supabase ESP32 code
└── firebase.c                # Legacy Firebase code
```

### Key Components

#### 1. SupabaseRealtimeService
```dart
// Initialize realtime subscriptions
final realtimeService = SupabaseRealtimeService();
await realtimeService.initialize();

// Listen to switch changes
realtimeService.switchStateStream.listen((data) {
  // Handle real-time switch updates
});

// Update switch state
await realtimeService.updateSwitchState(boardId, switchIndex, state);
```

#### 2. RealtimeSwitchWidget
```dart
RealtimeSwitchWidget(
  boardId: "BOARD_001",
  switchId: "BOARD_001_switch_1",
  switchIndex: 0,
  title: "Living Room Light",
  icon: Icons.lightbulb_outline,
  onStateChanged: (state) {
    print('Switch state changed: $state');
  },
)
```

#### 3. ESP32 Integration
```c
// Update switch state in Supabase
bool updateSwitchState(int switchIndex, bool state) {
    updateMainSwitchTable(switchIndex, state);    // SQL table
    updateRealtimeTable(switchIndex, state);      // Realtime table
    return true;
}
```

## 🔐 Security Features

### Authentication
- Email/password login
- Social login (Google, Apple)
- JWT token-based sessions
- Secure logout

### Row Level Security (RLS)
- Users can only access their own homes
- ESP32 devices have limited anon access
- Shared homes have controlled permissions

### API Security
- Rate limiting
- Input validation
- SQL injection prevention
- Secure key management

## 📊 Monitoring & Analytics

### Device Monitoring
- Online/offline status
- Last seen timestamps
- Connection quality
- Firmware versions

### Usage Analytics
- Switch usage patterns
- User activity logs
- Performance metrics
- Error tracking

## 🐛 Troubleshooting

### Common Issues

#### ESP32 Not Connecting
1. Check WiFi credentials
2. Verify Supabase URL and key
3. Reset configuration (7 quick button presses)
4. Check router firewall settings

#### App Not Updating
1. Verify internet connection
2. Check Supabase project status
3. Restart the app
4. Clear app cache

#### Realtime Not Working
1. Check Supabase Realtime is enabled
2. Verify table publications
3. Check RLS policies
4. Monitor network connectivity

### Debug Mode
Enable debug logging in the app:
```dart
import 'dart:developer';

// Add this for verbose logging
log('Debug message', name: 'SmartHome');
```

## 🤝 Contributing

### Development Setup
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

### Code Style
- Follow Dart/Flutter conventions
- Use meaningful variable names
- Comment complex logic
- Write unit tests

### ESP32 Development
- Use Arduino IDE style guide
- Document pin assignments
- Test on actual hardware
- Validate with multiple boards

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🆘 Support

### Documentation
- [Flutter Documentation](https://docs.flutter.dev/)
- [Supabase Documentation](https://supabase.com/docs)
- [ESP32 Arduino Documentation](https://docs.espressif.com/projects/arduino-esp32/)

### Community
- [Flutter Community](https://flutter.dev/community)
- [Supabase Discord](https://discord.supabase.com/)
- [ESP32 Forums](https://www.esp32.com/)

### Issues & Bug Reports
Please use the GitHub issue tracker for:
- Bug reports
- Feature requests
- Documentation improvements
- General questions

---

**Built with ❤️ using Flutter, Supabase, and ESP32**
