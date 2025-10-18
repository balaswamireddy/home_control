-- ============================================================================
-- CLEAN DATABASE SCHEMA FOR ESP32 SMART SWITCH
-- ============================================================================

-- Boards table
CREATE TABLE public.boards (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  mac_address TEXT,
  status TEXT DEFAULT 'offline',
  is_active BOOLEAN DEFAULT true,
  owner_id UUID,
  last_online TIMESTAMP WITH TIME ZONE,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Switches table
CREATE TABLE public.switches (
  id TEXT PRIMARY KEY,
  board_id TEXT NOT NULL REFERENCES public.boards(id) ON DELETE CASCADE,
  name TEXT NOT NULL,
  position INTEGER NOT NULL,
  state BOOLEAN DEFAULT false,
  is_enabled BOOLEAN DEFAULT true,
  last_state_change TIMESTAMP WITH TIME ZONE,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
  updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Device logs table (optional)
CREATE TABLE public.device_logs (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  switch_id TEXT REFERENCES public.switches(id) ON DELETE CASCADE,
  action TEXT NOT NULL,
  triggered_by TEXT,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- ============================================================================
-- ENABLE ROW LEVEL SECURITY
-- ============================================================================

ALTER TABLE public.boards ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.switches ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.device_logs ENABLE ROW LEVEL SECURITY;

-- ============================================================================
-- RLS POLICIES - ALLOW ESP32 (anon) TO ACCESS EVERYTHING
-- ============================================================================

-- Boards policies
CREATE POLICY "Allow anon to read boards"
ON public.boards FOR SELECT
TO anon
USING (true);

CREATE POLICY "Allow anon to update boards"
ON public.boards FOR UPDATE
TO anon
USING (true)
WITH CHECK (true);

-- Switches policies
CREATE POLICY "Allow anon to read switches"
ON public.switches FOR SELECT
TO anon
USING (true);

CREATE POLICY "Allow anon to update switches"
ON public.switches FOR UPDATE
TO anon
USING (true)
WITH CHECK (true);

-- Device logs policies
CREATE POLICY "Allow anon to insert logs"
ON public.device_logs FOR INSERT
TO anon
WITH CHECK (true);

CREATE POLICY "Allow anon to read logs"
ON public.device_logs FOR SELECT
TO anon
USING (true);

-- ============================================================================
-- INSERT TEST DATA FOR BOARD_001
-- ============================================================================

INSERT INTO public.boards (id, name, status, is_active)
VALUES ('BOARD_001', 'ESP32 Board 001', 'offline', true);

INSERT INTO public.switches (id, board_id, name, position, state, is_enabled)
VALUES 
  ('BOARD_001_switch_1', 'BOARD_001', 'Light 1', 0, false, true),
  ('BOARD_001_switch_2', 'BOARD_001', 'Light 2', 1, false, true),
  ('BOARD_001_switch_3', 'BOARD_001', 'Light 3', 2, false, true),
  ('BOARD_001_switch_4', 'BOARD_001', 'Light 4', 3, false, true);
