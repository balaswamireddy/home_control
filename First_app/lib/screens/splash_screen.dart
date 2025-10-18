import 'package:flutter/material.dart';
import 'package:flutter_spinkit/flutter_spinkit.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:supabase_flutter/supabase_flutter.dart';

class SplashScreen extends StatefulWidget {
  const SplashScreen({super.key});

  @override
  State<SplashScreen> createState() => _SplashScreenState();
}

class _SplashScreenState extends State<SplashScreen>
    with SingleTickerProviderStateMixin {
  late AnimationController _animationController;
  late Animation<double> _fadeAnimation;

  @override
  void initState() {
    super.initState();
    _animationController = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 2),
    );

    _fadeAnimation = Tween<double>(begin: 0.0, end: 1.0).animate(
      CurvedAnimation(parent: _animationController, curve: Curves.easeIn),
    );

    _animationController.forward();

    // Navigate after animation with onboarding check
    Future.delayed(const Duration(seconds: 3), () async {
      if (!mounted) return;

      try {
        // Check authentication status
        final user = Supabase.instance.client.auth.currentUser;
        print('🔍 Splash screen - Current user: ${user?.email ?? 'None'}');

        if (user != null) {
          // User is logged in, check tutorial status
          final prefs = await SharedPreferences.getInstance();
          final hasSeenTutorial = prefs.getBool('hasSeenTutorial') ?? false;

          print(
            '📱 Authenticated user navigation: ${hasSeenTutorial ? 'home' : 'onboarding-tutorial'}',
          );

          if (hasSeenTutorial) {
            Navigator.of(context).pushReplacementNamed('/home');
          } else {
            Navigator.of(context).pushReplacementNamed('/onboarding-tutorial');
          }
        } else {
          // User not logged in, check onboarding status
          final prefs = await SharedPreferences.getInstance();
          final onboardingCompleted =
              prefs.getBool('onboarding_completed') ?? false;

          print(
            '📱 Non-authenticated user navigation: ${onboardingCompleted ? 'auth' : 'onboarding'}',
          );

          if (onboardingCompleted) {
            // Onboarding completed, go to auth
            Navigator.of(context).pushReplacementNamed('/auth');
          } else {
            // First time user, show onboarding
            Navigator.of(context).pushReplacementNamed('/onboarding');
          }
        }
      } catch (e) {
        print('❌ Splash screen navigation error: $e');
        // Fallback to auth screen
        Navigator.of(context).pushReplacementNamed('/auth');
      }
    });
  }

  @override
  void dispose() {
    _animationController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      body: Center(
        child: FadeTransition(
          opacity: _fadeAnimation,
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Text(
                'CrudeLabs',
                style: GoogleFonts.montserrat(
                  fontSize: 32,
                  fontWeight: FontWeight.bold,
                  color: Colors.white,
                ),
              ),
              const SizedBox(height: 20),
              const SpinKitWave(color: Colors.white, size: 40.0),
            ],
          ),
        ),
      ),
    );
  }
}
