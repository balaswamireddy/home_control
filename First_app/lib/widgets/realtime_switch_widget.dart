import 'package:flutter/material.dart';
import 'dart:async';
import 'dart:developer';
import '../services/supabase_realtime_service.dart';

class RealtimeSwitchWidget extends StatefulWidget {
  final String boardId;
  final String switchId;
  final int switchIndex;
  final String title;
  final IconData icon;
  final bool enabled;
  final Function(bool)? onStateChanged;

  const RealtimeSwitchWidget({
    Key? key,
    required this.boardId,
    required this.switchId,
    required this.switchIndex,
    required this.title,
    this.icon = Icons.lightbulb_outline,
    this.enabled = true,
    this.onStateChanged,
  }) : super(key: key);

  @override
  State<RealtimeSwitchWidget> createState() => _RealtimeSwitchWidgetState();
}

class _RealtimeSwitchWidgetState extends State<RealtimeSwitchWidget>
    with TickerProviderStateMixin {
  bool _isOn = false;
  bool _isLoading = false;
  bool _isConnected = false;
  late StreamSubscription _switchSubscription;
  late StreamSubscription _deviceStatusSubscription;
  late AnimationController _rippleController;
  late AnimationController _glowController;
  late Animation<double> _rippleAnimation;
  late Animation<double> _glowAnimation;

  final SupabaseRealtimeService _realtimeService = SupabaseRealtimeService();

  @override
  void initState() {
    super.initState();
    _initializeAnimations();
    _initializeRealtimeSubscriptions();
    _loadInitialState();
  }

  void _initializeAnimations() {
    _rippleController = AnimationController(
      duration: const Duration(milliseconds: 300),
      vsync: this,
    );

    _glowController = AnimationController(
      duration: const Duration(seconds: 2),
      vsync: this,
    );

    _rippleAnimation = Tween<double>(begin: 0.0, end: 1.0).animate(
      CurvedAnimation(parent: _rippleController, curve: Curves.easeOut),
    );

    _glowAnimation = Tween<double>(begin: 0.5, end: 1.0).animate(
      CurvedAnimation(parent: _glowController, curve: Curves.easeInOut),
    );

    // Start glow animation when switch is on
    _glowController.repeat(reverse: true);
  }

  void _initializeRealtimeSubscriptions() {
    // Listen to switch state changes for this specific board
    _switchSubscription = _realtimeService.switchStateStream
        .where(
          (data) =>
              data['new'] != null &&
              data['new']['board_id'] == widget.boardId &&
              data['new']['switch_index'] == widget.switchIndex,
        )
        .listen((data) {
          if (mounted) {
            final newState = data['new']['state'] as bool? ?? false;
            if (newState != _isOn) {
              setState(() {
                _isOn = newState;
                _isLoading = false;
              });

              // Trigger ripple animation on state change
              _rippleController.forward().then((_) {
                _rippleController.reset();
              });

              // Call callback if provided
              widget.onStateChanged?.call(_isOn);

              log(
                'RealtimeSwitchWidget: Switch ${widget.switchIndex + 1} state updated to $_isOn',
              );
            }
          }
        });

    // Listen to device status changes
    _deviceStatusSubscription = _realtimeService.deviceStatusStream
        .where(
          (data) =>
              data['new'] != null && data['new']['board_id'] == widget.boardId,
        )
        .listen((data) {
          if (mounted) {
            final online = data['new']['online'] as bool? ?? false;
            setState(() {
              _isConnected = online;
            });
            log(
              'RealtimeSwitchWidget: Device ${widget.boardId} connection status: $online',
            );
          }
        });
  }

  void _loadInitialState() async {
    try {
      final switches = await _realtimeService.getSwitchStates(widget.boardId);
      final switchData = switches.firstWhere(
        (s) => s['switch_index'] == widget.switchIndex,
        orElse: () => {'state': false},
      );

      final deviceStatus = await _realtimeService.getDeviceStatus(
        widget.boardId,
      );

      if (mounted) {
        setState(() {
          _isOn = switchData['state'] ?? false;
          _isConnected = deviceStatus?['online'] ?? false;
        });
      }
    } catch (e) {
      log('RealtimeSwitchWidget: Error loading initial state - $e');
    }
  }

  Future<void> _toggleSwitch() async {
    if (!widget.enabled || _isLoading) return;

    setState(() {
      _isLoading = true;
    });

    try {
      final newState = !_isOn;
      final success = await _realtimeService.updateSwitchState(
        widget.boardId,
        widget.switchIndex,
        newState,
      );

      if (!success) {
        // Revert on failure
        setState(() {
          _isLoading = false;
        });
        _showError('Failed to update switch state');
      }
      // Success will be handled by the stream listener
    } catch (e) {
      setState(() {
        _isLoading = false;
      });
      _showError('Error updating switch: $e');
    }
  }

  void _showError(String message) {
    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(message),
          backgroundColor: Colors.red,
          duration: const Duration(seconds: 3),
        ),
      );
    }
  }

  @override
  void dispose() {
    _switchSubscription.cancel();
    _deviceStatusSubscription.cancel();
    _rippleController.dispose();
    _glowController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isDark = theme.brightness == Brightness.dark;

    return Card(
      elevation: _isOn ? 8 : 2,
      shadowColor: _isOn ? theme.primaryColor.withOpacity(0.3) : null,
      child: InkWell(
        onTap: _toggleSwitch,
        borderRadius: BorderRadius.circular(12),
        child: Container(
          padding: const EdgeInsets.all(16),
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(12),
            gradient: _isOn
                ? LinearGradient(
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                    colors: [
                      theme.primaryColor.withOpacity(0.1),
                      theme.primaryColor.withOpacity(0.05),
                    ],
                  )
                : null,
          ),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              // Connection Status Indicator
              Align(
                alignment: Alignment.topRight,
                child: Container(
                  width: 8,
                  height: 8,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: _isConnected ? Colors.green : Colors.red,
                  ),
                ),
              ),

              const SizedBox(height: 8),

              // Icon with animations
              Stack(
                alignment: Alignment.center,
                children: [
                  // Ripple effect
                  AnimatedBuilder(
                    animation: _rippleAnimation,
                    builder: (context, child) {
                      return Container(
                        width: 60 * _rippleAnimation.value,
                        height: 60 * _rippleAnimation.value,
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          color: theme.primaryColor.withOpacity(
                            0.3 * (1 - _rippleAnimation.value),
                          ),
                        ),
                      );
                    },
                  ),

                  // Glow effect for active switches
                  if (_isOn)
                    AnimatedBuilder(
                      animation: _glowAnimation,
                      builder: (context, child) {
                        return Container(
                          width: 50,
                          height: 50,
                          decoration: BoxDecoration(
                            shape: BoxShape.circle,
                            boxShadow: [
                              BoxShadow(
                                color: theme.primaryColor.withOpacity(
                                  0.4 * _glowAnimation.value,
                                ),
                                blurRadius: 20 * _glowAnimation.value,
                                spreadRadius: 5 * _glowAnimation.value,
                              ),
                            ],
                          ),
                        );
                      },
                    ),

                  // Main icon
                  Container(
                    width: 50,
                    height: 50,
                    decoration: BoxDecoration(
                      shape: BoxShape.circle,
                      color: _isOn
                          ? theme.primaryColor
                          : (isDark ? Colors.grey[700] : Colors.grey[300]),
                    ),
                    child: _isLoading
                        ? SizedBox(
                            width: 20,
                            height: 20,
                            child: CircularProgressIndicator(
                              strokeWidth: 2,
                              valueColor: AlwaysStoppedAnimation<Color>(
                                _isOn ? Colors.white : theme.primaryColor,
                              ),
                            ),
                          )
                        : Icon(
                            widget.icon,
                            color: _isOn
                                ? Colors.white
                                : (isDark ? Colors.white70 : Colors.black54),
                            size: 24,
                          ),
                  ),
                ],
              ),

              const SizedBox(height: 12),

              // Switch Title
              Text(
                widget.title,
                style: theme.textTheme.bodyMedium?.copyWith(
                  fontWeight: FontWeight.w500,
                  color: _isOn ? theme.primaryColor : null,
                ),
                textAlign: TextAlign.center,
                maxLines: 2,
                overflow: TextOverflow.ellipsis,
              ),

              const SizedBox(height: 8),

              // Status Text
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(12),
                  color: _isOn
                      ? theme.primaryColor.withOpacity(0.1)
                      : (isDark ? Colors.grey[800] : Colors.grey[200]),
                ),
                child: Text(
                  _isOn ? 'ON' : 'OFF',
                  style: theme.textTheme.bodySmall?.copyWith(
                    fontWeight: FontWeight.bold,
                    color: _isOn ? theme.primaryColor : null,
                  ),
                ),
              ),

              // Real-time indicator
              const SizedBox(height: 4),
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Container(
                    width: 4,
                    height: 4,
                    decoration: BoxDecoration(
                      shape: BoxShape.circle,
                      color: Colors.green.withOpacity(0.8),
                    ),
                  ),
                  const SizedBox(width: 4),
                  Text(
                    'Live',
                    style: theme.textTheme.bodySmall?.copyWith(
                      fontSize: 10,
                      color: Colors.green.withOpacity(0.8),
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}

// Grid widget for multiple switches
class RealtimeSwitchGrid extends StatefulWidget {
  final String boardId;
  final List<Map<String, dynamic>> switches;
  final Function(int, bool)? onSwitchChanged;

  const RealtimeSwitchGrid({
    Key? key,
    required this.boardId,
    required this.switches,
    this.onSwitchChanged,
  }) : super(key: key);

  @override
  State<RealtimeSwitchGrid> createState() => _RealtimeSwitchGridState();
}

class _RealtimeSwitchGridState extends State<RealtimeSwitchGrid> {
  final SupabaseRealtimeService _realtimeService = SupabaseRealtimeService();

  @override
  void initState() {
    super.initState();
    _initializeRealtimeService();
  }

  void _initializeRealtimeService() async {
    try {
      if (!_realtimeService.isInitialized) {
        await _realtimeService.initialize();
      }
    } catch (e) {
      log('RealtimeSwitchGrid: Error initializing realtime service - $e');
    }
  }

  @override
  Widget build(BuildContext context) {
    return GridView.builder(
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
        crossAxisCount: 2,
        childAspectRatio: 1.0,
        crossAxisSpacing: 12,
        mainAxisSpacing: 12,
      ),
      itemCount: widget.switches.length,
      itemBuilder: (context, index) {
        final switchData = widget.switches[index];
        return RealtimeSwitchWidget(
          boardId: widget.boardId,
          switchId: switchData['id'] ?? '',
          switchIndex: switchData['position'] ?? index,
          title: switchData['name'] ?? 'Switch ${index + 1}',
          icon: _getIconFromType(switchData['type']),
          enabled: switchData['is_enabled'] ?? true,
          onStateChanged: (state) {
            widget.onSwitchChanged?.call(index, state);
          },
        );
      },
    );
  }

  IconData _getIconFromType(String? type) {
    switch (type?.toLowerCase()) {
      case 'light':
        return Icons.lightbulb_outline;
      case 'fan':
        return Icons.air;
      case 'ac':
        return Icons.ac_unit;
      case 'heater':
        return Icons.local_fire_department;
      case 'plug':
        return Icons.power;
      default:
        return Icons.electrical_services;
    }
  }
}

// Device status widget
class RealtimeDeviceStatus extends StatefulWidget {
  final String boardId;

  const RealtimeDeviceStatus({Key? key, required this.boardId})
    : super(key: key);

  @override
  State<RealtimeDeviceStatus> createState() => _RealtimeDeviceStatusState();
}

class _RealtimeDeviceStatusState extends State<RealtimeDeviceStatus> {
  bool _isOnline = false;
  String? _lastSeen;
  String? _firmwareVersion;
  String? _ipAddress;
  late StreamSubscription _statusSubscription;
  final SupabaseRealtimeService _realtimeService = SupabaseRealtimeService();

  @override
  void initState() {
    super.initState();
    _initializeStatusListener();
    _loadInitialStatus();
  }

  void _initializeStatusListener() {
    _statusSubscription = _realtimeService
        .listenToBoardStatus(widget.boardId)
        .listen((status) {
          if (mounted && status != null) {
            setState(() {
              _isOnline = status['online'] ?? false;
              _lastSeen = status['last_seen'];
              _firmwareVersion = status['firmware_version'];
              _ipAddress = status['ip_address'];
            });
          }
        });
  }

  void _loadInitialStatus() async {
    try {
      final status = await _realtimeService.getDeviceStatus(widget.boardId);
      if (mounted && status != null) {
        setState(() {
          _isOnline = status['online'] ?? false;
          _lastSeen = status['last_seen'];
          _firmwareVersion = status['firmware_version'];
          _ipAddress = status['ip_address'];
        });
      }
    } catch (e) {
      log('RealtimeDeviceStatus: Error loading initial status - $e');
    }
  }

  @override
  void dispose() {
    _statusSubscription.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Container(
                  width: 12,
                  height: 12,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: _isOnline ? Colors.green : Colors.red,
                  ),
                ),
                const SizedBox(width: 8),
                Text(
                  _isOnline ? 'Device Online' : 'Device Offline',
                  style: theme.textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),
            if (_firmwareVersion != null) ...[
              Text('Firmware: $_firmwareVersion'),
              const SizedBox(height: 4),
            ],
            if (_ipAddress != null) ...[
              Text('IP Address: $_ipAddress'),
              const SizedBox(height: 4),
            ],
            if (_lastSeen != null) ...[
              Text('Last Seen: ${_formatDateTime(_lastSeen!)}'),
            ],
          ],
        ),
      ),
    );
  }

  String _formatDateTime(String dateTimeStr) {
    try {
      final dateTime = DateTime.parse(dateTimeStr);
      final now = DateTime.now();
      final difference = now.difference(dateTime);

      if (difference.inSeconds < 60) {
        return 'Just now';
      } else if (difference.inMinutes < 60) {
        return '${difference.inMinutes}m ago';
      } else if (difference.inHours < 24) {
        return '${difference.inHours}h ago';
      } else {
        return '${difference.inDays}d ago';
      }
    } catch (e) {
      return dateTimeStr;
    }
  }
}
