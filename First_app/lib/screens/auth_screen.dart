import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:supabase_flutter/supabase_flutter.dart';
import 'package:shared_preferences/shared_preferences.dart';

class AuthScreen extends StatefulWidget {
  const AuthScreen({super.key});

  @override
  State<AuthScreen> createState() => _AuthScreenState();
}

class _AuthScreenState extends State<AuthScreen> with TickerProviderStateMixin {
  final _formKey = GlobalKey<FormState>();
  final _emailController = TextEditingController();
  final _passwordController = TextEditingController();
  final _confirmPasswordController = TextEditingController();
  final _referralCodeController = TextEditingController();

  bool _isLoading = false;
  bool _isRegisterMode = false;
  bool _showReferralCode = false;
  bool _showConfirmPassword = false;

  late AnimationController _animationController;
  late Animation<double> _fadeAnimation;
  late Animation<Offset> _slideAnimation;

  @override
  void initState() {
    super.initState();
    _clearProblematicPreferences();

    // Initialize animations
    _animationController = AnimationController(
      duration: const Duration(milliseconds: 300),
      vsync: this,
    );

    _fadeAnimation = Tween<double>(begin: 0.0, end: 1.0).animate(
      CurvedAnimation(parent: _animationController, curve: Curves.easeInOut),
    );

    _slideAnimation =
        Tween<Offset>(begin: const Offset(0, 0.1), end: Offset.zero).animate(
          CurvedAnimation(
            parent: _animationController,
            curve: Curves.easeInOut,
          ),
        );
  }

  @override
  void dispose() {
    _animationController.dispose();
    _emailController.dispose();
    _passwordController.dispose();
    _confirmPasswordController.dispose();
    _referralCodeController.dispose();
    super.dispose();
  }

  Future<void> _clearProblematicPreferences() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove('has_seen_arrow_tutorial');

    final currentUser = Supabase.instance.client.auth.currentUser;
    if (currentUser != null) {
      print('Clearing stale authentication session...');
      await Supabase.instance.client.auth.signOut();
    }
  }

  Future<bool> _checkUserExists(String email) async {
    try {
      print('🔍 Checking if user exists: $email');

      // Check if user exists in the user_profiles table
      // This is more reliable than auth attempts
      final response = await Supabase.instance.client
          .from('user_profiles')
          .select('email')
          .eq('email', email)
          .maybeSingle();

      if (response != null) {
        print('✅ User exists in profiles: $email');
        return true;
      } else {
        print('❌ User does not exist in profiles: $email');
        return false;
      }
    } catch (e) {
      print('⚠️ Error checking user profiles: $e');

      // Fallback: Try the auth method
      try {
        await Supabase.instance.client.auth.signInWithPassword(
          email: email,
          password: 'wrong_password_check_12345',
        );

        // If no exception, user exists
        await Supabase.instance.client.auth.signOut();
        print('✅ User exists (auth fallback): $email');
        return true;
      } on AuthException catch (authError) {
        final errorMessage = authError.message.toLowerCase();
        print('🔍 Auth fallback error: ${authError.message}');

        // User doesn't exist
        if (errorMessage.contains('user not found') ||
            errorMessage.contains('email not found') ||
            errorMessage.contains('signup required')) {
          print('❌ User does not exist (auth fallback): $email');
          return false;
        }

        // User exists but wrong password
        print('✅ User exists, wrong password (auth fallback): $email');
        return true;
      } catch (fallbackError) {
        print('⚠️ Fallback error: $fallbackError');
        return false; // Default to registration
      }
    }
  }

  void _onEmailChanged() async {
    final email = _emailController.text.trim();

    // Reset state if email is empty or invalid
    if (email.isEmpty || !email.contains('@')) {
      setState(() {
        _isRegisterMode = false;
        _showConfirmPassword = false;
        _isLoading = false;
      });
      _animationController.reverse();
      return;
    }

    // Basic email validation
    final emailRegex = RegExp(r'^[^@]+@[^@]+\.[^@]+$');
    if (!emailRegex.hasMatch(email)) {
      return; // Don't check invalid emails
    }

    setState(() {
      _isLoading = true;
    });

    // Add small delay to avoid too many API calls
    await Future.delayed(const Duration(milliseconds: 500));

    // Check if email field still has the same value (user might have changed it)
    if (_emailController.text.trim() != email) {
      return; // Email changed, don't process this request
    }

    try {
      final userExists = await _checkUserExists(email);

      // Double-check the email hasn't changed during the API call
      if (_emailController.text.trim() == email && mounted) {
        print(
          '📱 UI Update: userExists=$userExists, isRegisterMode=${!userExists}',
        );
        setState(() {
          _isRegisterMode = !userExists;
          _showConfirmPassword = !userExists;
          _isLoading = false;
        });

        if (!userExists) {
          print('🎬 Showing registration fields (user does not exist)');
          _animationController.forward();
        } else {
          print('🎬 Hiding registration fields (user exists)');
          _animationController.reverse();
        }
      }
    } catch (e) {
      if (mounted) {
        setState(() {
          _isLoading = false;
        });
      }
    }
  }

  void _handleAuth() async {
    if (_formKey.currentState!.validate()) {
      if (_isRegisterMode &&
          _passwordController.text != _confirmPasswordController.text) {
        _showError('Passwords do not match');
        return;
      }

      setState(() {
        _isLoading = true;
      });

      try {
        if (_isRegisterMode) {
          await _register();
        } else {
          await _login();
        }
      } finally {
        if (mounted) {
          setState(() {
            _isLoading = false;
          });
        }
      }
    }
  }

  Future<void> _register() async {
    try {
      print('🔄 Starting registration for: ${_emailController.text.trim()}');

      // First, try to sign up the user with email confirmation disabled
      final response = await Supabase.instance.client.auth.signUp(
        email: _emailController.text.trim(),
        password: _passwordController.text,
        emailRedirectTo: null, // Disable email confirmation redirect
        data: {'email_confirmed': true, 'auto_confirm': true},
      );

      if (response.user != null) {
        print('✅ User registered successfully: ${response.user!.email}');

        // Wait a moment for the user to be created
        await Future.delayed(const Duration(milliseconds: 1000));

        // Now immediately sign in with the same credentials
        print('🔄 Signing in user after registration...');
        final loginResponse = await Supabase.instance.client.auth
            .signInWithPassword(
              email: _emailController.text.trim(),
              password: _passwordController.text,
            );

        if (loginResponse.user != null) {
          print('✅ Auto-login successful: ${loginResponse.user!.email}');

          // Create user profile
          await _createUserProfile(loginResponse.user!);

          // Show success message
          _showSuccess('Account created successfully! Welcome aboard!');

          // Navigate to app
          if (mounted) {
            await _checkAndNavigateAfterAuth();
          }
        } else {
          _showError(
            'Registration successful but login failed. Please try signing in manually.',
          );
        }
      } else {
        _showError('Registration failed: Unable to create account');
      }
    } on AuthException catch (e) {
      print('❌ Registration auth error: ${e.message}');

      // Handle email already registered case
      if (e.message.toLowerCase().contains('email already registered') ||
          e.message.toLowerCase().contains('user already registered') ||
          e.message.toLowerCase().contains('signup_disabled')) {
        // User already exists, try to log them in directly
        try {
          print('🔄 User exists, attempting direct login...');
          final loginResponse = await Supabase.instance.client.auth
              .signInWithPassword(
                email: _emailController.text.trim(),
                password: _passwordController.text,
              );

          if (loginResponse.user != null) {
            print('✅ Direct login successful for existing user');

            // Ensure user profile exists
            await _createUserProfile(loginResponse.user!);

            _showSuccess('Welcome back! Logged in successfully.');

            if (mounted) {
              await _checkAndNavigateAfterAuth();
            }
          } else {
            _showError(
              'User exists but login failed. Please check your password.',
            );
          }
        } catch (loginError) {
          _showError(
            'This email is already registered. Please check your password and try again.',
          );
        }
      } else if (e.message.toLowerCase().contains('email not confirmed') ||
          e.message.toLowerCase().contains('email confirmation required') ||
          e.message.toLowerCase().contains('confirm email')) {
        // Handle email confirmation requirement by attempting direct creation
        print(
          '🔄 Email confirmation required, attempting alternative approach...',
        );
        try {
          // Try direct login - sometimes the user is created but just needs confirmation bypass
          final loginResponse = await Supabase.instance.client.auth
              .signInWithPassword(
                email: _emailController.text.trim(),
                password: _passwordController.text,
              );

          if (loginResponse.user != null) {
            print('✅ Direct login successful despite confirmation requirement');
            await _createUserProfile(loginResponse.user!);
            _showSuccess('Account created successfully! Welcome aboard!');

            if (mounted) {
              await _checkAndNavigateAfterAuth();
            }
          } else {
            _showError(
              'Account created but requires email confirmation. Please check your email or contact support.',
            );
          }
        } catch (loginError) {
          _showError(
            'Account created but requires email confirmation. Please check your email or contact support.',
          );
        }
      } else {
        _showError('Registration failed: ${e.message}');
      }
    } catch (e) {
      print('❌ Registration general error: $e');
      _showError('Registration failed: ${e.toString()}');
    }
  }

  Future<void> _login() async {
    try {
      print('Attempting login for: ${_emailController.text.trim()}');

      final response = await Supabase.instance.client.auth.signInWithPassword(
        email: _emailController.text.trim(),
        password: _passwordController.text.trim(),
      );

      if (response.user != null && mounted) {
        await _verifyUserProfileAndNavigate(response.user!);
      } else {
        _showError('Login failed: Invalid credentials');
      }
    } on AuthException catch (e) {
      if (e.message.toLowerCase().contains('email not confirmed')) {
        _showEmailVerificationDialog();
      } else if (e.message.toLowerCase().contains(
            'invalid login credentials',
          ) ||
          e.message.toLowerCase().contains('invalid email or password') ||
          e.message.toLowerCase().contains('email not found')) {
        _showError(
          'Invalid email or password. Please check your credentials and try again.',
        );
      } else {
        _showError('Login failed: ${e.message}');
      }
    } catch (e) {
      _showError('Error: ${e.toString()}');
    }
  }

  Future<void> _createUserProfile(User user) async {
    try {
      print('🔄 Creating user profile for: ${user.email}');

      // Check if user profile already exists
      final existingProfile = await Supabase.instance.client
          .from('user_profiles')
          .select('id')
          .eq('id', user.id)
          .maybeSingle();

      if (existingProfile == null) {
        // Create new user profile
        await Supabase.instance.client.from('user_profiles').insert({
          'id': user.id,
          'username': user.email?.split('@')[0] ?? 'user',
          'email': user.email ?? '',
          'location': '',
          'created_at': DateTime.now().toIso8601String(),
        });
        print('✅ User profile created successfully for: ${user.email}');
      } else {
        print('ℹ️ User profile already exists for: ${user.email}');
      }
    } catch (e) {
      print('❌ Error creating user profile: $e');

      // Don't throw error here - just log it and continue
      // The user is already authenticated, we don't want to break the flow
      print(
        '⚠️ Continuing without profile creation - user can still access the app',
      );
    }
  }

  Future<void> _verifyUserProfileAndNavigate(User user) async {
    try {
      final profileResponse = await Supabase.instance.client
          .from('user_profiles')
          .select('username, email')
          .eq('id', user.id)
          .maybeSingle();

      if (profileResponse == null) {
        await Supabase.instance.client.auth.signOut();
        _showError(
          'Account not found. Please register first or contact support.',
        );
        return;
      }

      await _checkAndNavigateAfterAuth();
    } catch (e) {
      await Supabase.instance.client.auth.signOut();
      _showError('Login verification failed: ${e.toString()}');
    }
  }

  Future<void> _checkAndNavigateAfterAuth() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final hasSeenTutorial = prefs.getBool('hasSeenTutorial') ?? false;

      // Verify user is still authenticated
      final currentUser = Supabase.instance.client.auth.currentUser;
      if (currentUser == null) {
        _showError('Authentication failed. Please try again.');
        return;
      }

      print('✅ User authenticated successfully: ${currentUser.email}');
      print(
        '📱 Navigating to: ${hasSeenTutorial ? 'home' : 'onboarding-tutorial'}',
      );

      if (mounted) {
        if (!hasSeenTutorial) {
          Navigator.of(context).pushNamedAndRemoveUntil(
            '/onboarding-tutorial',
            (route) => false, // Remove all previous routes
          );
        } else {
          Navigator.of(context).pushNamedAndRemoveUntil(
            '/home',
            (route) => false, // Remove all previous routes
          );
        }
      }
    } catch (e) {
      print('❌ Navigation error: $e');
      _showError('Navigation failed: ${e.toString()}');
    }
  }

  void _showEmailVerificationDialog() {
    showDialog(
      context: context,
      builder: (BuildContext context) {
        return AlertDialog(
          title: const Text('Email Not Verified'),
          content: const Text(
            'Please check your email and click the verification link to activate your account.',
          ),
          actions: <Widget>[
            TextButton(
              child: const Text('OK'),
              onPressed: () => Navigator.of(context).pop(),
            ),
            TextButton(
              child: const Text('Resend Email'),
              onPressed: () async {
                try {
                  await Supabase.instance.client.auth.resend(
                    type: OtpType.signup,
                    email: _emailController.text.trim(),
                  );
                  if (mounted) {
                    _showSuccess('Verification email sent successfully!');
                    Navigator.of(context).pop();
                  }
                } catch (error) {
                  if (mounted) {
                    _showError('Failed to resend email: ${error.toString()}');
                  }
                }
              },
            ),
          ],
        );
      },
    );
  }

  Future<void> _joinHomeWithReferralCode() async {
    final referralCode = _referralCodeController.text.trim().toUpperCase();
    if (referralCode.isEmpty) {
      _showError('Please enter a referral code');
      return;
    }

    setState(() {
      _isLoading = true;
    });

    try {
      final shareResponse = await Supabase.instance.client
          .from('home_shares')
          .select('home_id, owner_id, expires_at')
          .eq('referral_code', referralCode)
          .eq('is_active', true)
          .maybeSingle();

      if (shareResponse == null) {
        _showError('Invalid or expired referral code');
        return;
      }

      final expiresAt = DateTime.parse(shareResponse['expires_at']);
      if (expiresAt.isBefore(DateTime.now())) {
        _showError('This referral code has expired');
        return;
      }

      _showSuccess(
        'Valid referral code! Complete authentication to join the home.',
      );
    } catch (e) {
      _showError('Error: ${e.toString()}');
    } finally {
      if (mounted) {
        setState(() {
          _isLoading = false;
        });
      }
    }
  }

  void _showError(String message) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(message), backgroundColor: Colors.red),
    );
  }

  void _showSuccess(String message) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(message), backgroundColor: Colors.green),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.transparent,
      body: SafeArea(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(16.0),
          child: Form(
            key: _formKey,
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const SizedBox(
                  height: 60,
                ), // Add top space for better centering
                Text(
                  _isRegisterMode ? 'Create Account' : 'Welcome Back',
                  style: GoogleFonts.montserrat(
                    fontSize: 32,
                    fontWeight: FontWeight.bold,
                  ),
                  textAlign: TextAlign.center,
                ),
                const SizedBox(height: 8),
                Text(
                  _isLoading && _emailController.text.isNotEmpty
                      ? 'Checking email...'
                      : _isRegisterMode
                      ? 'New user - Join the smart home experience'
                      : 'Existing user - Sign in to your account',
                  style: GoogleFonts.montserrat(
                    fontSize: 16,
                    color: _isLoading && _emailController.text.isNotEmpty
                        ? Colors.orange[600]
                        : _isRegisterMode
                        ? Colors.green[600]
                        : Colors.blue[600],
                  ),
                  textAlign: TextAlign.center,
                ),
                const SizedBox(height: 40),

                // Email Field
                TextFormField(
                  controller: _emailController,
                  keyboardType: TextInputType.emailAddress,
                  decoration: InputDecoration(
                    labelText: 'Email',
                    border: const OutlineInputBorder(),
                    prefixIcon: const Icon(Icons.email_outlined),
                    suffixIcon: _isLoading
                        ? const SizedBox(
                            width: 20,
                            height: 20,
                            child: Padding(
                              padding: EdgeInsets.all(12.0),
                              child: CircularProgressIndicator(strokeWidth: 2),
                            ),
                          )
                        : null,
                  ),
                  onChanged: (_) => _onEmailChanged(),
                  validator: (value) {
                    if (value == null || value.isEmpty) {
                      return 'Please enter your email';
                    }
                    if (!value.contains('@')) {
                      return 'Please enter a valid email';
                    }
                    return null;
                  },
                ),
                const SizedBox(height: 16),

                // Password Field
                TextFormField(
                  controller: _passwordController,
                  obscureText: true,
                  decoration: const InputDecoration(
                    labelText: 'Password',
                    border: OutlineInputBorder(),
                    prefixIcon: Icon(Icons.lock_outlined),
                  ),
                  validator: (value) {
                    if (value == null || value.isEmpty) {
                      return 'Please enter your password';
                    }
                    if (_isRegisterMode && value.length < 6) {
                      return 'Password must be at least 6 characters';
                    }
                    return null;
                  },
                ),

                // Animated Confirm Password Field
                if (_showConfirmPassword)
                  SlideTransition(
                    position: _slideAnimation,
                    child: FadeTransition(
                      opacity: _fadeAnimation,
                      child: Padding(
                        padding: const EdgeInsets.only(top: 16.0),
                        child: TextFormField(
                          controller: _confirmPasswordController,
                          obscureText: true,
                          decoration: const InputDecoration(
                            labelText: 'Confirm Password',
                            border: OutlineInputBorder(),
                            prefixIcon: Icon(Icons.lock_outlined),
                          ),
                          validator: (value) {
                            if (_isRegisterMode &&
                                (value == null || value.isEmpty)) {
                              return 'Please confirm your password';
                            }
                            return null;
                          },
                        ),
                      ),
                    ),
                  ),

                const SizedBox(height: 16),

                // Referral Code Toggle
                Row(
                  children: [
                    Checkbox(
                      value: _showReferralCode,
                      onChanged: (value) {
                        setState(() {
                          _showReferralCode = value ?? false;
                        });
                      },
                    ),
                    const Text('I have a referral code'),
                  ],
                ),

                // Animated Referral Code Field
                if (_showReferralCode) ...[
                  const SizedBox(height: 16),
                  TextFormField(
                    controller: _referralCodeController,
                    decoration: const InputDecoration(
                      labelText: 'Referral Code',
                      hintText: 'Enter 8-character code',
                      border: OutlineInputBorder(),
                      prefixIcon: Icon(Icons.card_giftcard),
                    ),
                    textCapitalization: TextCapitalization.characters,
                    maxLength: 8,
                  ),
                  const SizedBox(height: 8),
                  SizedBox(
                    width: double.infinity,
                    child: OutlinedButton(
                      onPressed: _isLoading ? null : _joinHomeWithReferralCode,
                      style: OutlinedButton.styleFrom(
                        padding: const EdgeInsets.symmetric(vertical: 16),
                      ),
                      child: const Text('Join Home with Code'),
                    ),
                  ),
                ],

                const SizedBox(height: 24),

                // Dynamic Auth Button
                ElevatedButton(
                  onPressed: _isLoading ? null : _handleAuth,
                  style: ElevatedButton.styleFrom(
                    padding: const EdgeInsets.symmetric(vertical: 16),
                    backgroundColor: _isRegisterMode
                        ? Colors.green
                        : Colors.blue,
                  ),
                  child: _isLoading
                      ? const CircularProgressIndicator(color: Colors.white)
                      : Text(
                          _isRegisterMode ? 'Register' : 'Sign In',
                          style: const TextStyle(
                            fontSize: 16,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                ),

                const SizedBox(height: 16),

                // Dynamic Footer Text
                Text(
                  _isRegisterMode
                      ? 'By creating an account, you agree to our terms and conditions.'
                      : 'Enter your email above to get started or create a new account.',
                  style: TextStyle(fontSize: 12, color: Colors.grey[600]),
                  textAlign: TextAlign.center,
                ),
                const SizedBox(height: 40), // Add bottom spacing for keyboard
              ],
            ),
          ),
        ),
      ),
    );
  }
}
