#pragma once
#include <regex>
#include "../../SDK/SDK.h"
#include <chrono>

class timer {
public:
	typedef std::chrono::system_clock clock;
	std::chrono::time_point<clock> last{};
	inline timer() {};

	inline bool check(unsigned ms) const
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - last).count() >= ms;
	}
	inline bool test_and_set(unsigned ms)
	{
		if (check(ms))
		{
			update();
			return true;
		}
		return false;
	}
	inline void update()
	{
		last = clock::now();
	}
};

enum nospread_sync_state_t {
	NOT_SYNCED = 0,
	CORRECTING,
	SYNCED,
	DEAD_SYNC,
};

class c_seed_prediction {
public:
	//setup vars
	bool m_waiting_perf_data = false;
	bool m_should_update_time = false;
	bool m_waiting_for_post_SNM = false;
	bool m_resync_needed = false;
	bool m_is_syncing = false;
	bool m_last_was_player_perf = false;
	bool m_resynced_this_death = false;
	nospread_sync_state_t m_no_spread_synced = NOT_SYNCED; // this will be set to 0 each level init / level shutdown
	bool m_bad_mantissa = false;      // Also reset every levelinit/shutdown
	double m_float_time_delta = 0.0;
	double m_ping_at_send = 0.0;
	double m_last_ping_at_send = 0.0;
	double m_sent_client_floattime = 0.0;
	double m_last_correction = 0.0;
	double m_write_usercmd_correction = 0.0;
	double m_last_sync_delta_time = 0.0;
	float m_prediction_seed = 0.0;
	float m_time_start = 0.0f;
	float m_mantissa_step = 0.0f;
	float m_server_time = 0.0f;
	int m_new_packets = 0;
	bool m_use_usercmd_seed = false;
	float m_current_weapon_spread = 0.0;
	bool m_first_usercmd = false;
	CUserCmd m_user_cmd_backup;
	bool m_called_from_sendmove = false;
	bool m_should_update_usercmd_correction = false;
	//now functions.

	float calculate_mantissa_step(float value);
	float server_cur_time();
	bool is_perfect_shot(CBaseCombatWeapon* weapon, float provided_time);
	bool dispatch_user_message_handler(bf_read* buf, int type);
	void apply_spread_correction(Vector& angles, int seed, float spread);
	bool send_net_message_handler(INetMessage* data);
	void send_net_message_post_handler();
	void cl_sendmove_handler();
	void cl_sendmove_handler_post();
	void reset_variables();
	void create_move_handler(CUserCmd* cmd);
};

ADD_FEATURE(c_seed_prediction, seedpred)