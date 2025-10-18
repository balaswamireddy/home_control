# Supabase Hybrid Architecture: SQL Tables + Realtime Database

## Overview

This project has evolved from a Firebase Realtime Database architecture to a **hybrid Supabase architecture** that combines the power of SQL tables with real-time functionality. This approach provides the best of both worlds:

- **SQL Tables**: For complex queries, relationships, user management, and data integrity
- **Realtime Tables**: For instant synchronization between ESP32 devices and Flutter app

## Architecture Diagram

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   ESP32 Device  │    │   Flutter App   │    │   Supabase DB   │
│                 │    │                 │    │                 │
│  Physical       │    │  UI Controls    │    │  SQL Tables:    │
│  Switches   ────┼────┼─── Widgets  ────┼────┤  • homes        │
│                 │    │                 │    │  • rooms        │
│  WiFi + HTTP    │    │  Realtime       │    │  • boards       │
│  REST API   ────┼────┼─── Service  ────┼────┤  • switches     │
│                 │    │                 │    │  • users        │
│  Auto-sync  ────┼────┼─── Streams  ────┼────┤  • timers       │
│                 │    │                 │    │                 │
└─────────────────┘    └─────────────────┘    │  Realtime:      │
                                              │  • realtime_sw  │
                                              │  • device_status│
                                              └─────────────────┘
```

## Database Schema

### Core SQL Tables

#### 1. **`homes`** - User's Home Management
```sql
CREATE TABLE public.homes (
  id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
  user_id UUID NOT NULL REFERENCES auth.users(id),
  name TEXT NOT NULL,
  description TEXT,
  address TEXT,
  timezone TEXT DEFAULT 'UTC',
  is_primary BOOLEAN DEFAULT false,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);
```

#### 2. **`rooms`** - Room Organization
```sql
CREATE TABLE public.rooms (
  id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
  home_id UUID NOT NULL REFERENCES public.homes(id),
  name TEXT NOT NULL,
  icon TEXT DEFAULT 'meeting_room',
  display_order INTEGER DEFAULT 0,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);
```

#### 3. **`boards`** - ESP32 Device Management
```sql
CREATE TABLE public.boards (
  id TEXT PRIMARY KEY,  -- "BOARD_001", "BOARD_002", etc.
  home_id UUID REFERENCES public.homes(id),
  room_id UUID REFERENCES public.rooms(id),
  owner_id UUID REFERENCES auth.users(id),
  name TEXT NOT NULL,
  mac_address TEXT,
  status TEXT DEFAULT 'offline',
  firmware_version TEXT,
  last_online TIMESTAMP WITH TIME ZONE,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);
```

#### 4. **`switches`** - Individual Switch Control
```sql
CREATE TABLE public.switches (
  id TEXT PRIMARY KEY,  -- "BOARD_001_switch_1", etc.
  board_id TEXT NOT NULL REFERENCES public.boards(id),
  name TEXT NOT NULL,
  type TEXT DEFAULT 'light',
  position INTEGER NOT NULL,  -- 0, 1, 2, 3 for 4-switch boards
  state BOOLEAN DEFAULT false,
  is_enabled BOOLEAN DEFAULT true,
  last_state_change TIMESTAMP WITH TIME ZONE,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);
```

### Realtime Tables (for ESP32 Communication)

#### 1. **`realtime_switches`** - Real-time Switch States
```sql
CREATE TABLE public.realtime_switches (
  id TEXT PRIMARY KEY,
  board_id TEXT NOT NULL,
  switch_index INTEGER NOT NULL,
  state BOOLEAN DEFAULT false,
  last_updated TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  UNIQUE(board_id, switch_index)
);
```

#### 2. **`device_status`** - Real-time Device Status
```sql
CREATE TABLE public.device_status (
  board_id TEXT PRIMARY KEY,
  online BOOLEAN DEFAULT false,
  last_seen TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  firmware_version TEXT,
  ip_address TEXT,
  metadata JSONB DEFAULT '{}'
);
```

## Data Synchronization

### Automatic Sync Triggers

The system uses PostgreSQL triggers to keep SQL tables and realtime tables synchronized:

```sql
-- Sync switches table with realtime_switches
CREATE OR REPLACE FUNCTION sync_realtime_switches()
RETURNS TRIGGER AS $$
BEGIN
  IF TG_OP = 'INSERT' OR TG_OP = 'UPDATE' THEN
    INSERT INTO public.realtime_switches (id, board_id, switch_index, state, last_updated)
    VALUES (NEW.id, NEW.board_id, NEW.position, NEW.state, NOW())
    ON CONFLICT (id) DO UPDATE SET
      state = EXCLUDED.state,
      last_updated = EXCLUDED.last_updated;
    RETURN NEW;
  END IF;
  RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER sync_switches_to_realtime
  AFTER INSERT OR UPDATE ON public.switches
  FOR EACH ROW EXECUTE FUNCTION sync_realtime_switches();
```

## ESP32 Implementation

### Supabase Integration

The ESP32 code (`supabase_esp32.c`) provides:

1. **WiFi Configuration Portal**: Easy setup via captive portal
2. **REST API Communication**: Direct HTTP calls to Supabase
3. **Dual Table Updates**: Updates both main and realtime tables
4. **Real-time Polling**: Checks for remote changes every 5 seconds
5. **Heartbeat System**: Keeps device status updated

### Key Features:

```c
// Update both tables for consistency
bool updateSwitchState(String boardId, int switchIndex, bool state) {
    // Update realtime table for instant sync
    updateRealtimeTable(switchIndex, state);
    
    // Update main table for data persistence
    updateMainSwitchTable(switchIndex, state);
}

// Firebase-style compatibility
bool setFirebaseStylePath(String path, dynamic value) {
    // Converts "/2025Saaa08/switchs/switch1" to Supabase format
    // Maintains compatibility with existing Firebase code
}
```

## Flutter Implementation

### Realtime Service

The `SupabaseRealtimeService` provides:

1. **Real-time Subscriptions**: Live updates from database
2. **Stream-based Updates**: Reactive programming model
3. **Firebase Compatibility**: Support for Firebase-style paths
4. **Error Handling**: Robust error recovery

### Key Components:

```dart
class SupabaseRealtimeService {
  // Stream controllers for real-time data
  Stream<Map<String, dynamic>> get switchStateStream;
  Stream<Map<String, dynamic>> get deviceStatusStream;
  
  // Update methods
  Future<bool> updateSwitchState(String boardId, int switchIndex, bool state);
  Future<bool> updateDeviceStatus(String boardId, bool online);
  
  // Firebase compatibility
  Future<bool> setFirebaseStylePath(String path, dynamic value);
  Future<dynamic> getFirebaseStylePath(String path);
}
```

### UI Widgets

The `RealtimeSwitchWidget` provides:

1. **Live State Updates**: Instant visual feedback
2. **Connection Status**: Shows device online/offline status
3. **Visual Animations**: Ripple effects and glow animations
4. **Error Handling**: User-friendly error messages

## Migration from Firebase

### What Changed:

1. **Database Structure**: 
   - Firebase: `/2025Saaa08/switchs/switch1`
   - Supabase: `realtime_switches` table with structured data

2. **Authentication**:
   - Firebase: Email/password auth
   - Supabase: Built-in auth with RLS policies

3. **Real-time Updates**:
   - Firebase: Real-time database listeners
   - Supabase: PostgreSQL change data capture

### Compatibility Layer:

The system maintains Firebase-style path compatibility:

```dart
// Firebase style (still works)
await realtimeService.setFirebaseStylePath("/BOARD_001/switchs/switch1", true);

// Supabase native (recommended)
await realtimeService.updateSwitchState("BOARD_001", 0, true);
```

## Benefits of Hybrid Approach

### SQL Tables Benefits:
- ✅ **Complex Queries**: JOIN operations across homes, rooms, boards
- ✅ **Data Integrity**: Foreign key constraints and validation
- ✅ **User Management**: Built-in authentication and authorization
- ✅ **Scalability**: Efficient indexing and query optimization
- ✅ **Reporting**: Complex analytics and reporting queries

### Realtime Tables Benefits:
- ⚡ **Instant Updates**: Sub-second synchronization
- 🔄 **Live Sync**: ESP32 ↔ Flutter app communication
- 📱 **Responsive UI**: Immediate visual feedback
- 🌐 **Multi-device**: Sync across multiple app instances
- 🔋 **Efficient**: Minimal data transfer for status updates

## Usage Examples

### Flutter App Integration:

```dart
// Initialize the realtime service
final realtimeService = SupabaseRealtimeService();
await realtimeService.initialize();

// Create realtime switch widgets
RealtimeSwitchGrid(
  boardId: "BOARD_001",
  switches: switchesData,
  onSwitchChanged: (index, state) {
    print('Switch $index changed to $state');
  },
)

// Listen to device status
StreamBuilder<Map<String, dynamic>?>(
  stream: realtimeService.listenToBoardStatus("BOARD_001"),
  builder: (context, snapshot) {
    final isOnline = snapshot.data?['online'] ?? false;
    return Text(isOnline ? 'Online' : 'Offline');
  },
)
```

### ESP32 Usage:

1. **Physical Button Press** → `controlRelay()` → `queueDatabaseUpdate()`
2. **Background Task** → `processPendingUpdate()` → Updates both tables
3. **Remote Change** → `pollSupabaseForChanges()` → `controlRelay()`
4. **Status Updates** → `sendHeartbeat()` → Updates device status

## Security Features

### Row Level Security (RLS):

```sql
-- Allow ESP32 (anon) access for device communication
CREATE POLICY "Allow anon to read switches"
ON public.switches FOR SELECT TO anon, authenticated
USING (true);

-- Users can only access their own homes
CREATE POLICY "Users can view their own homes"
ON public.homes FOR SELECT TO authenticated
USING (user_id = auth.uid());
```

### API Key Management:
- ESP32 uses anon key for device operations
- Flutter app uses authenticated sessions
- Service role key for admin operations (server-side only)

## Performance Optimizations

1. **Indexes**: Optimized for common queries
2. **Connection Pooling**: Efficient database connections
3. **Caching**: Local state caching in widgets
4. **Debouncing**: Prevents excessive updates
5. **Background Tasks**: Non-blocking ESP32 operations

## Monitoring and Debugging

### Real-time Monitoring:
- Device online/offline status
- Last seen timestamps
- Connection quality metrics
- Switch state change logs

### Debug Features:
- Console logging in ESP32
- Stream debugging in Flutter
- Database query logging
- Error tracking and recovery

## Future Enhancements

1. **Edge Functions**: Server-side logic for complex operations
2. **Storage Integration**: File uploads for device images
3. **Push Notifications**: Real-time alerts
4. **Analytics**: Usage patterns and insights
5. **API Gateway**: Rate limiting and request optimization

---

This hybrid architecture provides a robust, scalable solution that combines the reliability of SQL databases with the responsiveness of real-time systems, perfect for IoT applications requiring both complex data management and instant synchronization.