import 'dart:ui';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import '../models/switch_model.dart';
import '../models/switch_type.dart';
import '../widgets/animated_switch_icon.dart';
import '../widgets/glass_widgets.dart';

class GlassSwitchControlTile extends StatefulWidget {
  final SwitchDevice device;
  final Function(bool) onToggle;
  final Function(SwitchType) onTypeChanged;
  final Function(String) onNameChanged;

  const GlassSwitchControlTile({
    Key? key,
    required this.device,
    required this.onToggle,
    required this.onTypeChanged,
    required this.onNameChanged,
  }) : super(key: key);

  @override
  State<GlassSwitchControlTile> createState() => _GlassSwitchControlTileState();
}

class _GlassSwitchControlTileState extends State<GlassSwitchControlTile> {
  static final Map<String, DateTime> _globalToggleTimes =
      {}; // Global debounce across all switches
  static const int _debounceMs = 800; // Aggressive 800ms debounce

  Color _getTypeColor() {
    switch (widget.device.type) {
      case SwitchType.light:
        return Colors.amber;
      case SwitchType.fan:
        return Colors.blue;
      case SwitchType.ac:
        return Colors.cyan;
      case SwitchType.heater:
        return Colors.orange;
      case SwitchType.tv:
        return Colors.purple;
      case SwitchType.speaker:
        return Colors.green;
      case SwitchType.plug:
        return Colors.red;
      case SwitchType.motor:
        return Colors.indigo;
      case SwitchType.pump:
        return Colors.teal;
      case SwitchType.door:
        return Colors.brown;
      case SwitchType.window:
        return Colors.lightBlue;
      case SwitchType.curtain:
        return Colors.deepPurple;
    }
  }

  List<Color> _getGradientColors() {
    final baseColor = _getTypeColor();
    return [baseColor.withOpacity(0.8), baseColor.withOpacity(0.6)];
  }

  // Simple, bulletproof toggle with aggressive debouncing
  void _handleToggle([bool? targetValue]) {
    final now = DateTime.now();
    final switchId = widget.device.id;

    // Check global debounce for this specific switch
    if (_globalToggleTimes.containsKey(switchId)) {
      final timeDiff = now
          .difference(_globalToggleTimes[switchId]!)
          .inMilliseconds;
      if (timeDiff < _debounceMs) {
        print('🚫 Switch $switchId blocked: ${timeDiff}ms < ${_debounceMs}ms');
        return;
      }
    }

    // Record this toggle time
    _globalToggleTimes[switchId] = now;

    print('✅ Switch $switchId toggle allowed');

    // Single haptic feedback
    HapticFeedback.lightImpact();

    // Execute toggle with target state or current toggle
    final newState = targetValue ?? !widget.device.state;
    widget.onToggle(newState);
  }

  Future<void> _showNameEditDialog(BuildContext context) async {
    final TextEditingController nameController = TextEditingController(
      text: widget.device.name,
    );

    return showDialog(
      context: context,
      barrierColor: Colors.black.withOpacity(0.3),
      builder: (context) => BackdropFilter(
        filter: ImageFilter.blur(sigmaX: 10, sigmaY: 10),
        child: Dialog(
          backgroundColor: Colors.transparent,
          child: GlassCard(
            padding: const EdgeInsets.all(24),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(
                  'Edit Switch Name',
                  style: Theme.of(context).textTheme.titleLarge?.copyWith(
                    color: Colors.black,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 20),
                Container(
                  height: 45, // Slightly smaller box height
                  child: TextField(
                    controller: nameController,
                    style: const TextStyle(
                      color: Colors.black,
                      fontSize: 12, // Smaller text inside the field
                    ),
                    decoration: InputDecoration(
                      labelText: 'Switch Name',
                      labelStyle: TextStyle(
                        color: Colors.black.withOpacity(0.7),
                        fontSize: 16, // Larger label size
                        fontWeight: FontWeight
                            .w600, // Slightly bold for better visibility
                      ),
                      hintText: 'Enter new name',
                      hintStyle: TextStyle(
                        color: Colors.black.withOpacity(0.5),
                        fontSize: 12, // Smaller hint size to match input text
                      ),
                      fillColor: Colors.white,
                      filled: true,
                      contentPadding: const EdgeInsets.symmetric(
                        horizontal: 12,
                        vertical: 8, // Reduced padding for smaller box
                      ),
                      border: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(
                          10,
                        ), // Slightly smaller radius
                        borderSide: BorderSide(
                          color: Colors.grey.withOpacity(0.3),
                        ),
                      ),
                      enabledBorder: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(10),
                        borderSide: BorderSide(
                          color: Colors.grey.withOpacity(0.3),
                        ),
                      ),
                      focusedBorder: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(10),
                        borderSide: BorderSide(
                          color: Colors.blue.withOpacity(0.6),
                        ),
                      ),
                    ),
                    autofocus: true,
                  ),
                ),
                const SizedBox(height: 24),
                Row(
                  children: [
                    Expanded(
                      child: GlassButton(
                        onPressed: () => Navigator.pop(context),
                        child: const Text(
                          'Cancel',
                          style: TextStyle(color: Colors.white),
                        ),
                      ),
                    ),
                    const SizedBox(width: 16),
                    Expanded(
                      child: GlassButton(
                        onPressed: () {
                          final newName = nameController.text.trim();
                          if (newName.isNotEmpty &&
                              newName != widget.device.name) {
                            widget.onNameChanged(newName);
                          }
                          Navigator.pop(context);
                        },
                        gradientColors: _getGradientColors(),
                        child: const Text(
                          'Save',
                          style: TextStyle(color: Colors.white),
                        ),
                      ),
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    // Cache expensive computations to reduce lag
    final typeColor = _getTypeColor();
    final gradientColors = _getGradientColors();
    final isDeviceOn = widget.device.state;
    final deviceName = widget.device.name;
    final deviceTypeName = widget.device.type.name.toUpperCase();

    return RepaintBoundary(
      // Optimize repainting
      child: GlassCard(
        padding: const EdgeInsets.all(8),
        customShadows: [
          BoxShadow(
            color: Colors.black.withOpacity(0.1),
            blurRadius: 15,
            offset: const Offset(0, 8),
          ),
          if (isDeviceOn)
            BoxShadow(
              color: typeColor.withOpacity(0.3),
              blurRadius: 20,
              spreadRadius: 2,
            ),
        ],
        // Remove onTap from card to prevent double toggle
        child: Column(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            // Tappable area (everything except bottom row)
            Expanded(
              child: GestureDetector(
                onTap: () => _handleToggle(),
                behavior: HitTestBehavior.opaque,
                child: Column(
                  children: [
                    // Top row with icon and menu button
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        // Icon with glow effect
                        Container(
                          width: 32,
                          height: 32,
                          decoration: BoxDecoration(
                            shape: BoxShape.circle,
                            gradient: LinearGradient(
                              colors: isDeviceOn
                                  ? gradientColors
                                  : [
                                      Colors.grey.withOpacity(0.3),
                                      Colors.grey.withOpacity(0.1),
                                    ],
                              begin: Alignment.topLeft,
                              end: Alignment.bottomRight,
                            ),
                            boxShadow: [
                              if (isDeviceOn)
                                BoxShadow(
                                  color: typeColor.withOpacity(0.4),
                                  blurRadius: 12,
                                  spreadRadius: 2,
                                ),
                            ],
                          ),
                          child: Center(
                            child: AnimatedSwitchIcon(
                              type: widget.device.type,
                              isOn: isDeviceOn,
                              size: 18,
                            ),
                          ),
                        ),
                        // Type selector button
                        GlassButton(
                          padding: const EdgeInsets.all(4),
                          child: Icon(
                            Icons.more_vert,
                            color: Colors.white.withOpacity(0.8),
                            size: 16,
                          ),
                          onPressed: () => _showTypeSelector(context),
                        ),
                      ],
                    ),

                    const SizedBox(height: 4),

                    // Switch name and type - Tappable area for toggle
                    GestureDetector(
                      onTap: () => _handleToggle(
                        !widget.device.state,
                      ), // Handle tap for entire name/type area
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          GestureDetector(
                            onLongPress: () => _showNameEditDialog(context),
                            child: Text(
                              deviceName, // Use cached value
                              style: Theme.of(context).textTheme.titleSmall
                                  ?.copyWith(
                                    color: Colors.white,
                                    fontWeight: FontWeight.bold,
                                    fontSize: 12,
                                  ),
                              maxLines: 1,
                              overflow: TextOverflow.ellipsis,
                            ),
                          ),
                          const SizedBox(height: 2),
                          Text(
                            deviceTypeName, // Use cached value
                            style: Theme.of(context).textTheme.bodySmall
                                ?.copyWith(
                                  color: Colors.white.withOpacity(0.7),
                                  letterSpacing: 0.5,
                                  fontSize: 8,
                                ),
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
            ),

            const SizedBox(height: 4),

            // Bottom row with status and toggle
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                // Status indicator
                Container(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 6,
                    vertical: 2,
                  ),
                  decoration: BoxDecoration(
                    color: isDeviceOn
                        ? typeColor.withOpacity(0.2)
                        : Colors.grey.withOpacity(0.2),
                    borderRadius: BorderRadius.circular(10),
                    border: Border.all(
                      color: isDeviceOn
                          ? typeColor.withOpacity(0.4)
                          : Colors.grey.withOpacity(0.4),
                    ),
                  ),
                  child: Text(
                    isDeviceOn ? 'ON' : 'OFF',
                    style: TextStyle(
                      color: isDeviceOn ? typeColor : Colors.grey,
                      fontSize: 8,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                ),
                // Glass toggle switch
                GestureDetector(
                  onTap: () {
                    // Prevent card tap when toggle is tapped
                  },
                  child: GlassToggleSwitch(
                    value: isDeviceOn,
                    onChanged: (value) {
                      // Only the toggle switch should handle this
                      _handleToggle(value);
                    },
                    activeColors: gradientColors,
                    width: 40,
                    height: 20,
                  ),
                ),
              ],
            ),
          ],
        ),
      ), // Close GlassCard
    ); // Close RepaintBoundary
  }

  void _showTypeSelector(BuildContext context) {
    showModalBottomSheet(
      context: context,
      backgroundColor: Colors.transparent,
      barrierColor: Colors.black.withOpacity(0.3),
      isScrollControlled: true,
      builder: (context) => BackdropFilter(
        filter: ImageFilter.blur(sigmaX: 10, sigmaY: 10),
        child: Container(
          margin: const EdgeInsets.all(16),
          constraints: BoxConstraints(
            maxHeight: MediaQuery.of(context).size.height * 0.7,
          ),
          child: GlassCard(
            padding: const EdgeInsets.all(24),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                // Handle bar
                Container(
                  width: 40,
                  height: 4,
                  decoration: BoxDecoration(
                    color: Colors.white.withOpacity(0.5),
                    borderRadius: BorderRadius.circular(2),
                  ),
                ),
                const SizedBox(height: 20),

                Text(
                  'Switch Type',
                  style: Theme.of(context).textTheme.titleLarge?.copyWith(
                    color: Colors.white,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 24),

                // Scrollable list of switch types
                Flexible(
                  child: SingleChildScrollView(
                    child: Column(
                      children: SwitchType.values.map((type) {
                        final isSelected = type == widget.device.type;
                        return Container(
                          margin: const EdgeInsets.only(bottom: 12),
                          child: GlassButton(
                            onPressed: () {
                              widget.onTypeChanged(type);
                              Navigator.pop(context);
                            },
                            gradientColors: isSelected
                                ? _getGradientColors()
                                : null,
                            child: Row(
                              children: [
                                AnimatedSwitchIcon(
                                  key: ValueKey('selector_${type.name}'),
                                  type: type,
                                  isOn: true,
                                  size: 24,
                                ),
                                const SizedBox(width: 16),
                                Text(
                                  type.name.toUpperCase(),
                                  style: TextStyle(
                                    color: Colors.white,
                                    fontWeight: isSelected
                                        ? FontWeight.bold
                                        : FontWeight.normal,
                                  ),
                                ),
                                const Spacer(),
                                if (isSelected)
                                  Icon(
                                    Icons.check_circle,
                                    color: Colors.white,
                                    size: 20,
                                  ),
                              ],
                            ),
                          ),
                        );
                      }).toList(),
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
