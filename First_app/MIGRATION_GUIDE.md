# Migration Guide: Firebase → Supabase Hybrid Architecture

## Overview

This guide helps you migrate your existing Firebase Realtime Database smart home system to the new Supabase hybrid architecture that combines SQL tables with realtime functionality.

## Migration Benefits

### What You Gain
- ✅ **Better Performance**: SQL queries are faster than Firebase queries
- ✅ **Data Relationships**: Foreign key constraints and JOINs
- ✅ **Cost Efficiency**: Supabase pricing is more predictable
- ✅ **SQL Power**: Complex queries, aggregations, and analytics
- ✅ **Type Safety**: PostgreSQL's strong typing system
- ✅ **Realtime + SQL**: Best of both worlds
- ✅ **Better Auth**: Built-in Row Level Security (RLS)

### What's Preserved
- 🔄 **Realtime Updates**: Still instant synchronization
- 📱 **UI Behavior**: Same user experience
- 🔧 **ESP32 Compatibility**: Physical devices work the same
- 🏠 **Feature Parity**: All existing features maintained

## Step-by-Step Migration

### 1. Database Schema Migration

#### Firebase Structure (Old)
```
firebase-project/
└── 2025Saaa08/
    ├── switchs/
    │   ├── switch1: true/false
    │   ├── switch2: true/false
    │   └── switch3: true/false
    └── deviceStatus/
        └── online: true/false
```

#### Supabase Structure (New)
```sql
-- Main SQL Tables
homes (id, user_id, name, address...)
rooms (id, home_id, name, icon...)
boards (id, home_id, name, status...)
switches (id, board_id, name, position, state...)

-- Realtime Tables (for ESP32)
realtime_switches (id, board_id, switch_index, state...)
device_status (board_id, online, last_seen...)
```

### 2. Code Migration

#### ESP32 Code Changes

**Firebase Code (Old):**
```c
// Firebase connection
Firebase.begin(&config, &auth);

// Read switch state
Firebase.RTDB.getBool(&fbdo, "/2025Saaa08/switchs/switch1");

// Update switch state  
Firebase.RTDB.setBool(&fbdo, "/2025Saaa08/switchs/switch1", true);

// Set device status
Firebase.RTDB.setBool(&fbdo, "/deviceStatus/online", true);
```

**Supabase Code (New):**
```c
// HTTP REST API calls
HTTPClient http;
WiFiClientSecure client;

// Read switch states
String url = supabase_url + "/rest/v1/realtime_switches?board_id=eq.BOARD_001";
http.begin(client, url);
http.addHeader("apikey", supabase_key);
int response = http.GET();

// Update switch state (updates both tables automatically)
url = supabase_url + "/rest/v1/switches?id=eq.BOARD_001_switch_1";
http.sendRequest("PATCH", "{\"state\": true}");

// Update device status
url = supabase_url + "/rest/v1/device_status";
http.POST("{\"board_id\": \"BOARD_001\", \"online\": true}");
```

#### Flutter App Changes

**Firebase Code (Old):**
```dart
// Firebase initialization
await Firebase.initializeApp();
final database = FirebaseDatabase.instance;

// Listen to changes
database.ref('/2025Saaa08/switchs').onValue.listen((event) {
  // Handle switch changes
});

// Update switch
await database.ref('/2025Saaa08/switchs/switch1').set(true);
```

**Supabase Code (New):**
```dart
// Supabase initialization
await Supabase.initialize(url: url, anonKey: key);
final realtimeService = SupabaseRealtimeService();
await realtimeService.initialize();

// Listen to changes
realtimeService.switchStateStream.listen((data) {
  // Handle switch changes
});

// Update switch
await realtimeService.updateSwitchState("BOARD_001", 0, true);
```

### 3. Migration Checklist

#### Pre-Migration Setup
- [ ] Create Supabase project
- [ ] Run database schema script
- [ ] Test database connectivity
- [ ] Backup existing Firebase data
- [ ] Prepare ESP32 with new firmware

#### Data Migration
- [ ] Export current device configurations
- [ ] Map Firebase paths to Supabase tables
- [ ] Import existing switch configurations
- [ ] Verify data integrity
- [ ] Test realtime functionality

#### Code Migration
- [ ] Update ESP32 firmware
- [ ] Migrate Flutter app services
- [ ] Update UI components
- [ ] Test all features
- [ ] Verify error handling

#### Deployment
- [ ] Deploy updated Flutter app
- [ ] Update ESP32 devices
- [ ] Monitor for issues
- [ ] Document changes
- [ ] Train users if needed

### 4. Data Migration Script

#### Export Firebase Data
```javascript
// Export existing Firebase data
const admin = require('firebase-admin');
const fs = require('fs');

admin.initializeApp({
  credential: admin.credential.cert(serviceAccount),
  databaseURL: 'https://your-project.firebaseio.com'
});

const db = admin.database();

// Export device configurations
db.ref('/').once('value', (snapshot) => {
  const data = snapshot.val();
  fs.writeFileSync('firebase-export.json', JSON.stringify(data, null, 2));
});
```

#### Import to Supabase
```sql
-- Create temporary table for import
CREATE TEMP TABLE firebase_import (
  device_id TEXT,
  switch_data JSONB
);

-- Import data (run this after loading your export)
INSERT INTO boards (id, name, status)
SELECT 
  device_id,
  'Migrated Device ' || device_id,
  'offline'
FROM firebase_import;

-- Create switches from Firebase data
INSERT INTO switches (id, board_id, name, position, state)
SELECT 
  device_id || '_switch_' || (ordinality),
  device_id,
  'Switch ' || ordinality,
  ordinality - 1,
  (switch_data->('switch' || ordinality))::boolean
FROM firebase_import, 
     generate_series(1, 4) WITH ORDINALITY;
```

### 5. Compatibility Layer

For gradual migration, you can use a compatibility layer:

#### Firebase-Style Service (Temporary)
```dart
class FirebaseCompatibilityService {
  final SupabaseRealtimeService _supabase = SupabaseRealtimeService();

  // Firebase-style methods
  Future<void> setValue(String path, dynamic value) async {
    await _supabase.setFirebaseStylePath(path, value);
  }

  Stream<dynamic> onValue(String path) {
    return _supabase.switchStateStream
        .where((data) => _matchesPath(data, path))
        .map((data) => _extractValue(data, path));
  }

  bool _matchesPath(Map<String, dynamic> data, String path) {
    // Convert Firebase paths to Supabase data matching
    // Implementation details...
  }
}
```

### 6. Testing Migration

#### Unit Tests
```dart
void main() {
  group('Migration Tests', () {
    test('Switch state sync', () async {
      final service = SupabaseRealtimeService();
      await service.initialize();
      
      // Test switch update
      await service.updateSwitchState("BOARD_001", 0, true);
      
      // Verify both tables updated
      final switches = await service.getSwitchStates("BOARD_001");
      expect(switches[0]['state'], true);
    });
    
    test('Firebase compatibility', () async {
      final service = SupabaseRealtimeService();
      
      // Test Firebase-style path
      await service.setFirebaseStylePath("/BOARD_001/switchs/switch1", true);
      final value = await service.getFirebaseStylePath("/BOARD_001/switchs/switch1");
      
      expect(value, true);
    });
  });
}
```

#### Integration Tests
```dart
void main() {
  group('ESP32 Integration', () {
    test('Device communication', () async {
      // Test ESP32 → Supabase → Flutter flow
      // 1. Simulate ESP32 HTTP request
      // 2. Verify database update
      // 3. Check Flutter receives realtime update
    });
  });
}
```

### 7. Monitoring Migration

#### Health Checks
```sql
-- Check data consistency
SELECT 
  s.id,
  s.state as sql_state,
  rs.state as realtime_state,
  s.state = rs.state as consistent
FROM switches s
LEFT JOIN realtime_switches rs ON s.id = rs.id
WHERE s.state != rs.state;

-- Monitor realtime activity
SELECT 
  board_id,
  COUNT(*) as updates_count,
  MAX(last_updated) as last_activity
FROM realtime_switches
WHERE last_updated > NOW() - INTERVAL '1 hour'
GROUP BY board_id;
```

#### Performance Metrics
```dart
class MigrationMetrics {
  static void trackSwitchUpdate(String boardId, Duration latency) {
    // Track response times
    print('Switch update latency: ${latency.inMilliseconds}ms');
  }

  static void trackRealtimeLatency(Duration latency) {
    // Monitor realtime update delays
    print('Realtime latency: ${latency.inMilliseconds}ms');
  }
}
```

### 8. Rollback Plan

#### Emergency Rollback
If issues arise, you can rollback to Firebase:

1. **Keep Firebase Project Active** during migration
2. **Switch ESP32 firmware** back to Firebase version
3. **Revert Flutter app** to Firebase services
4. **Update DNS/endpoints** if needed

#### Rollback Script
```bash
#!/bin/bash
# Emergency rollback script

echo "Starting rollback to Firebase..."

# Revert ESP32 firmware
echo "Flash Firebase firmware to ESP32 devices"
# esptool.py --chip esp32 write_flash 0x1000 firebase_firmware.bin

# Revert Flutter app
echo "Deploy Firebase version of Flutter app"
# flutter build apk --flavor firebase
# Deploy to app stores

echo "Rollback complete"
```

### 9. Post-Migration Validation

#### Functional Tests
- [ ] All switches respond to physical buttons
- [ ] Flutter app shows real-time updates
- [ ] Device status updates correctly
- [ ] User authentication works
- [ ] Home/room management functions
- [ ] Timer functionality operates
- [ ] Alexa integration works

#### Performance Tests
- [ ] Switch response time < 500ms
- [ ] Realtime updates < 200ms delay
- [ ] App startup time acceptable
- [ ] Database query performance
- [ ] ESP32 memory usage stable

#### User Acceptance
- [ ] UI/UX unchanged for users
- [ ] All existing features work
- [ ] No data loss occurred
- [ ] Performance improved or maintained

### 10. Migration Timeline

#### Phase 1: Preparation (Week 1)
- Set up Supabase project
- Create database schema
- Develop migration scripts
- Test on development environment

#### Phase 2: Development (Week 2-3)
- Migrate ESP32 firmware
- Update Flutter services
- Create compatibility layers
- Comprehensive testing

#### Phase 3: Pilot Testing (Week 4)
- Deploy to test devices
- Monitor performance
- Gather feedback
- Fix any issues

#### Phase 4: Production Migration (Week 5)
- Deploy to production
- Monitor closely
- Be ready for rollback
- Support user issues

### 11. Success Criteria

#### Technical Success
- ✅ All devices online and responsive
- ✅ Realtime updates working
- ✅ No data corruption
- ✅ Performance meets or exceeds Firebase
- ✅ All features functional

#### User Success
- ✅ No user-visible changes
- ✅ Same or better performance
- ✅ No additional user training needed
- ✅ Positive user feedback

### 12. Support During Migration

#### User Communication
```
Subject: Smart Home System Upgrade

Dear Users,

We're upgrading our smart home system to provide better performance and new features. During the migration:

- Your devices will continue working normally
- Some brief interruptions may occur (< 5 minutes)
- All your settings and schedules are preserved
- Contact support if you experience any issues

The upgrade provides:
- Faster response times
- Better reliability
- New features coming soon

Thank you for your patience.
```

#### Support Checklist
- [ ] Support team trained on new system
- [ ] Migration documentation ready
- [ ] Rollback procedures tested
- [ ] User communication sent
- [ ] Monitoring dashboard active

---

**Migration Tips:**
- Test thoroughly in development first
- Migrate gradually (pilot → full deployment)
- Keep Firebase running during transition
- Monitor closely for first 48 hours
- Be prepared for immediate rollback if needed