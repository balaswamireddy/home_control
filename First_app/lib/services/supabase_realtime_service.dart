import 'dart:async';
import 'dart:developer';
import 'package:supabase_flutter/supabase_flutter.dart';

class SupabaseRealtimeService {
  static final SupabaseRealtimeService _instance =
      SupabaseRealtimeService._internal();
  factory SupabaseRealtimeService() => _instance;
  SupabaseRealtimeService._internal();

  final SupabaseClient _client = Supabase.instance.client;

  // Stream controllers for real-time data
  final StreamController<Map<String, dynamic>> _switchStateController =
      StreamController<Map<String, dynamic>>.broadcast();
  final StreamController<Map<String, dynamic>> _deviceStatusController =
      StreamController<Map<String, dynamic>>.broadcast();
  final StreamController<Map<String, dynamic>> _boardController =
      StreamController<Map<String, dynamic>>.broadcast();

  // Subscriptions
  RealtimeChannel? _switchesChannel;
  RealtimeChannel? _deviceStatusChannel;
  RealtimeChannel? _boardsChannel;

  // Getters for streams
  Stream<Map<String, dynamic>> get switchStateStream =>
      _switchStateController.stream;
  Stream<Map<String, dynamic>> get deviceStatusStream =>
      _deviceStatusController.stream;
  Stream<Map<String, dynamic>> get boardStream => _boardController.stream;

  bool _isInitialized = false;

  /// Initialize realtime subscriptions
  Future<void> initialize() async {
    if (_isInitialized) return;

    try {
      await _subscribeToSwitches();
      await _subscribeToDeviceStatus();
      await _subscribeToBoards();

      _isInitialized = true;
      log('SupabaseRealtimeService: Initialized successfully');
    } catch (e) {
      log('SupabaseRealtimeService: Error initializing - $e');
      rethrow;
    }
  }

  /// Subscribe to switch state changes
  Future<void> _subscribeToSwitches() async {
    try {
      _switchesChannel = _client
          .channel('realtime_switches')
          .onPostgresChanges(
            event: PostgresChangeEvent.all,
            schema: 'public',
            table: 'realtime_switches',
            callback: (payload) {
              _handleSwitchChange(payload);
            },
          );

      await _switchesChannel!.subscribe();
      log('SupabaseRealtimeService: Subscribed to switches');
    } catch (e) {
      log('SupabaseRealtimeService: Error subscribing to switches - $e');
      rethrow;
    }
  }

  /// Subscribe to device status changes
  Future<void> _subscribeToDeviceStatus() async {
    try {
      _deviceStatusChannel = _client
          .channel('device_status')
          .onPostgresChanges(
            event: PostgresChangeEvent.all,
            schema: 'public',
            table: 'device_status',
            callback: (payload) {
              _handleDeviceStatusChange(payload);
            },
          );

      await _deviceStatusChannel!.subscribe();
      log('SupabaseRealtimeService: Subscribed to device status');
    } catch (e) {
      log('SupabaseRealtimeService: Error subscribing to device status - $e');
      rethrow;
    }
  }

  /// Subscribe to board changes
  Future<void> _subscribeToBoards() async {
    try {
      _boardsChannel = _client
          .channel('boards')
          .onPostgresChanges(
            event: PostgresChangeEvent.all,
            schema: 'public',
            table: 'boards',
            callback: (payload) {
              _handleBoardChange(payload);
            },
          );

      await _boardsChannel!.subscribe();
      log('SupabaseRealtimeService: Subscribed to boards');
    } catch (e) {
      log('SupabaseRealtimeService: Error subscribing to boards - $e');
      rethrow;
    }
  }

  /// Handle switch state changes
  void _handleSwitchChange(PostgresChangePayload payload) {
    try {
      final data = {
        'event': payload.eventType.name,
        'table': 'realtime_switches',
        'new': payload.newRecord,
        'old': payload.oldRecord,
        'timestamp': DateTime.now().toIso8601String(),
      };

      _switchStateController.add(data);

      log('SupabaseRealtimeService: Switch change - ${payload.eventType.name}');
      log('SupabaseRealtimeService: Switch data - ${payload.newRecord}');
    } catch (e) {
      log('SupabaseRealtimeService: Error handling switch change - $e');
    }
  }

  /// Handle device status changes
  void _handleDeviceStatusChange(PostgresChangePayload payload) {
    try {
      final data = {
        'event': payload.eventType.name,
        'table': 'device_status',
        'new': payload.newRecord,
        'old': payload.oldRecord,
        'timestamp': DateTime.now().toIso8601String(),
      };

      _deviceStatusController.add(data);

      log(
        'SupabaseRealtimeService: Device status change - ${payload.eventType.name}',
      );
      log('SupabaseRealtimeService: Device data - ${payload.newRecord}');
    } catch (e) {
      log('SupabaseRealtimeService: Error handling device status change - $e');
    }
  }

  /// Handle board changes
  void _handleBoardChange(PostgresChangePayload payload) {
    try {
      final data = {
        'event': payload.eventType.name,
        'table': 'boards',
        'new': payload.newRecord,
        'old': payload.oldRecord,
        'timestamp': DateTime.now().toIso8601String(),
      };

      _boardController.add(data);

      log('SupabaseRealtimeService: Board change - ${payload.eventType.name}');
      log('SupabaseRealtimeService: Board data - ${payload.newRecord}');
    } catch (e) {
      log('SupabaseRealtimeService: Error handling board change - $e');
    }
  }

  /// Update switch state via realtime (for ESP32 compatibility)
  Future<bool> updateSwitchState(
    String boardId,
    int switchIndex,
    bool state,
  ) async {
    try {
      final switchId = '${boardId}_switch_${switchIndex + 1}';

      // Update both tables for consistency
      await _client.from('realtime_switches').upsert({
        'id': switchId,
        'board_id': boardId,
        'switch_index': switchIndex,
        'state': state,
        'last_updated': DateTime.now().toIso8601String(),
      });

      // Also update main switches table
      await _client
          .from('switches')
          .update({
            'state': state,
            'last_state_change': DateTime.now().toIso8601String(),
          })
          .eq('id', switchId);

      log('SupabaseRealtimeService: Switch state updated - $switchId: $state');
      return true;
    } catch (e) {
      log('SupabaseRealtimeService: Error updating switch state - $e');
      return false;
    }
  }

  /// Update device status
  Future<bool> updateDeviceStatus(
    String boardId,
    bool online, {
    Map<String, dynamic>? metadata,
  }) async {
    try {
      await _client.from('device_status').upsert({
        'board_id': boardId,
        'online': online,
        'last_seen': DateTime.now().toIso8601String(),
        if (metadata != null) 'metadata': metadata,
      });

      // Also update main boards table
      await _client
          .from('boards')
          .update({
            'status': online ? 'online' : 'offline',
            'last_online': DateTime.now().toIso8601String(),
          })
          .eq('id', boardId);

      log('SupabaseRealtimeService: Device status updated - $boardId: $online');
      return true;
    } catch (e) {
      log('SupabaseRealtimeService: Error updating device status - $e');
      return false;
    }
  }

  /// Get current switch states for a board
  Future<List<Map<String, dynamic>>> getSwitchStates(String boardId) async {
    try {
      final response = await _client
          .from('realtime_switches')
          .select()
          .eq('board_id', boardId)
          .order('switch_index');

      return List<Map<String, dynamic>>.from(response);
    } catch (e) {
      log('SupabaseRealtimeService: Error getting switch states - $e');
      return [];
    }
  }

  /// Get device status
  Future<Map<String, dynamic>?> getDeviceStatus(String boardId) async {
    try {
      final response = await _client
          .from('device_status')
          .select()
          .eq('board_id', boardId)
          .single();

      return response;
    } catch (e) {
      log('SupabaseRealtimeService: Error getting device status - $e');
      return null;
    }
  }

  /// Listen to specific board's switches
  Stream<List<Map<String, dynamic>>> listenToBoardSwitches(String boardId) {
    return switchStateStream
        .where(
          (data) => data['new'] != null && data['new']['board_id'] == boardId,
        )
        .asyncMap((_) => getSwitchStates(boardId));
  }

  /// Listen to specific board's status
  Stream<Map<String, dynamic>?> listenToBoardStatus(String boardId) {
    return deviceStatusStream
        .where(
          (data) => data['new'] != null && data['new']['board_id'] == boardId,
        )
        .map((data) => data['new'] as Map<String, dynamic>?);
  }

  /// Firebase-style path updates for ESP32 compatibility
  Future<bool> setFirebaseStylePath(String path, dynamic value) async {
    try {
      // Parse Firebase-style paths like "/2025Saaa08/switchs/switch1"
      final parts = path.split('/').where((p) => p.isNotEmpty).toList();

      if (parts.length >= 3 && parts[1] == 'switchs') {
        // Extract board ID and switch info
        final boardId = parts[0];
        final switchName = parts[2]; // "switch1", "switch2", etc.
        final switchIndex = int.parse(switchName.replaceAll('switch', '')) - 1;

        return await updateSwitchState(boardId, switchIndex, value as bool);
      } else if (parts.length >= 2 && parts[1] == 'online') {
        // Device status update
        final boardId = parts[0];
        return await updateDeviceStatus(boardId, value as bool);
      }

      log('SupabaseRealtimeService: Unsupported Firebase path - $path');
      return false;
    } catch (e) {
      log('SupabaseRealtimeService: Error setting Firebase-style path - $e');
      return false;
    }
  }

  /// Firebase-style path reads for ESP32 compatibility
  Future<dynamic> getFirebaseStylePath(String path) async {
    try {
      final parts = path.split('/').where((p) => p.isNotEmpty).toList();

      if (parts.length >= 3 && parts[1] == 'switchs') {
        // Get switch state
        final boardId = parts[0];
        final switchName = parts[2];
        final switchIndex = int.parse(switchName.replaceAll('switch', '')) - 1;

        final switches = await getSwitchStates(boardId);
        final switchData = switches.firstWhere(
          (s) => s['switch_index'] == switchIndex,
          orElse: () => {'state': false},
        );

        return switchData['state'];
      } else if (parts.length >= 2 && parts[1] == 'online') {
        // Get device status
        final boardId = parts[0];
        final status = await getDeviceStatus(boardId);
        return status?['online'] ?? false;
      }

      return null;
    } catch (e) {
      log('SupabaseRealtimeService: Error getting Firebase-style path - $e');
      return null;
    }
  }

  /// Dispose of all resources
  Future<void> dispose() async {
    try {
      await _switchesChannel?.unsubscribe();
      await _deviceStatusChannel?.unsubscribe();
      await _boardsChannel?.unsubscribe();

      await _switchStateController.close();
      await _deviceStatusController.close();
      await _boardController.close();

      _isInitialized = false;
      log('SupabaseRealtimeService: Disposed successfully');
    } catch (e) {
      log('SupabaseRealtimeService: Error disposing - $e');
    }
  }

  /// Check if service is initialized
  bool get isInitialized => _isInitialized;
}
