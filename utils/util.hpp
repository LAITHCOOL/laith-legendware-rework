#pragma once

#include "..\includes.hpp"
#include "..\sdk\math\Vector.hpp"
#include "..\sdk\misc\CUserCmd.hpp"
#include "..\..\utils\crypt_str.h"

class entity_t;
class player_t;
class matrix3x4_t;
class IClientEntity;
class CGameTrace;
class IMaterial;
class CTraceFilter;
class c_baseplayeranimationstate;
class C_CSGOPlayerAnimationState;

class Address {
protected:
	uintptr_t m_addr;

public:
	// default ctor/dtor.
	__forceinline  Address() : m_addr{} {};
	__forceinline ~Address() {};

	// ctors.
	__forceinline Address(uintptr_t a) : m_addr{ a } {}
	__forceinline Address(const void* a) : m_addr{ (uintptr_t)a } {}

	// arithmetic operators for native types.
	__forceinline operator uintptr_t() { return m_addr; }
	__forceinline operator void* () { return (void*)m_addr; }
	__forceinline operator const void* () { return (const void*)m_addr; }

	// is-equals-operator.
	__forceinline bool operator==(Address a) const {
		return as< uintptr_t >() == a.as< uintptr_t >();
	}
	__forceinline bool operator!=(Address a) const {
		return as< uintptr_t >() != a.as< uintptr_t >();
	}

	// cast / add offset and cast.
	template< typename t = Address >
	__forceinline t as() const {
		return (m_addr) ? (t)m_addr : t{};
	}

	template< typename t = Address >
	__forceinline t as(size_t offset) const {
		return (m_addr) ? (t)(m_addr + offset) : t{};
	}

	template< typename t = Address >
	__forceinline t as(ptrdiff_t offset) const {
		return (m_addr) ? (t)(m_addr + offset) : t{};
	}

	// add offset and dereference.
	template< typename t = Address >
	__forceinline t at(size_t offset) const {
		return (m_addr) ? *(t*)(m_addr + offset) : t{};
	}

	template< typename t = Address >
	__forceinline t at(ptrdiff_t offset) const {
		return (m_addr) ? *(t*)(m_addr + offset) : t{};
	}

	// add offset.
	template< typename t = Address >
	__forceinline t add(size_t offset) const {
		return (m_addr) ? (t)(m_addr + offset) : t{};
	}

	template< typename t = Address >
	__forceinline t add(ptrdiff_t offset) const {
		return (m_addr) ? (t)(m_addr + offset) : t{};
	}

	// subtract offset.
	template< typename t = Address >
	__forceinline t sub(size_t offset) const {
		return (m_addr) ? (t)(m_addr - offset) : t{};
	}

	template< typename t = Address >
	__forceinline t sub(ptrdiff_t offset) const {
		return (m_addr) ? (t)(m_addr - offset) : t{};
	}

	// dereference.
	template< typename t = Address >
	__forceinline t to() const {
		return *(t*)m_addr;
	}

	// verify adddress and dereference n times.
	template< typename t = Address >
	__forceinline t get(size_t n = 1) {
		uintptr_t out;

		if (!m_addr)
			return t{};

		out = m_addr;

		for (size_t i{ n }; i > 0; --i) {
			// can't dereference, return null.
			if (!valid(out))
				return t{};

			out = *(uintptr_t*)out;
		}

		return (t)out;
	}

	// follow relative8 and relative16/32 offsets.
	template< typename t = Address >
	__forceinline t rel8(size_t offset) {
		uintptr_t   out;
		uint8_t     r;

		if (!m_addr)
			return t{};

		out = m_addr + offset;

		// get relative offset.
		r = *(uint8_t*)out;
		if (!r)
			return t{};

		// relative to address of next instruction.
		// short jumps can go forward and backward depending on the size of the second byte.
		// if the second byte is below 128, the jmp goes forwards.
		// if the second byte is above 128, the jmp goes backwards ( subtract two's complement of the relative offset from the address of the next instruction ).
		if (r < 128)
			out = (out + 1) + r;
		else
			out = (out + 1) - (uint8_t)(~r + 1);

		return (t)out;
	}

	template< typename t = Address >
	__forceinline t rel32(size_t offset) {
		uintptr_t   out;
		uint32_t    r;

		if (!m_addr)
			return t{};

		out = m_addr + offset;

		// get rel32 offset.
		r = *(uint32_t*)out;
		if (!r)
			return t{};

		// relative to address of next instruction.
		out = (out + 4) + r;

		return (t)out;
	}

	// set.
	template< typename t = uintptr_t > __forceinline void set(const t& value) {
		if (!m_addr)
			return;

		*(t*)m_addr = value;
	}

	// checks if address is not null and has correct page protection.
	static __forceinline bool valid(uintptr_t addr) {
		MEMORY_BASIC_INFORMATION mbi;

		// check for invalid address.
		if (!addr)
			return false;

		//// check for invalid page protection.
		//if (!g_winapi.VirtualQuery((const void*)addr, &mbi, sizeof(mbi)))
		//	return false;

		// todo - dex; fix this, its wrong... check for rwe or something too
		if ( /*!( mbi.State & MEM_COMMIT ) ||*/ (mbi.Protect & PAGE_NOACCESS) || (mbi.Protect & PAGE_GUARD))
			return false;

		return true;
	}

	// relative virtual address.
	template< typename t = Address >
	static __forceinline t RVA(Address base, size_t offset) {
		return base.as< t >(offset);
	}
};
struct datamap_t;

struct Box
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

struct HPInfo
{
	int hp = -1;
	int hp_difference = 0;
	float hp_difference_time = 0.0f;
};

struct SoundInfo
{
	float last_time = FLT_MIN;
	Vector origin = ZERO;
};

namespace util
{
	uintptr_t find_pattern(const char* module_name, const char* pattern, const char* mask);
	uint64_t FindSignature(const char* szModule, const char* szSignature);
	template < typename t = Address >
	__forceinline static t get_method(Address this_ptr, size_t index) {
		return (t)this_ptr.to< t* >()[index];
	}
	bool visible(const Vector& start, const Vector& end, entity_t* entity, player_t* from);
	bool is_button_down(int code);

	int epoch_time();
	void RotateMovement(CUserCmd* cmd, float yaw);
	bool get_bbox(entity_t* e, Box& box, bool player_esp);
	void trace_line(Vector& start, Vector& end, unsigned int mask, CTraceFilter* filter, CGameTrace* tr);
	void clip_trace_to_players(IClientEntity* e, const Vector& start, const Vector& end, unsigned int mask, CTraceFilter* filter, CGameTrace* tr);
	bool is_valid_hitgroup(int index);
	bool is_breakable_entity(IClientEntity* e);
	int get_hitbox_by_hitgroup(int index);
	void movement_fix_new(Vector& wish_angle, CUserCmd* m_pcmd);
	void MovementFix(Vector& wish_angle, CUserCmd* m_pcmd);
	void movement_fix(Vector& wish_angle, CUserCmd* m_pcmd);
	unsigned int find_in_datamap(datamap_t* map, const char* name);
	void color_modulate(float color[3], IMaterial* material);
	bool get_backtrack_matrix(player_t* e, matrix3x4_t* matrix);
	void create_state(c_baseplayeranimationstate* state, player_t* e);
	void create_state1(C_CSGOPlayerAnimationState* state, player_t* e);
	void update_state(c_baseplayeranimationstate* state, const Vector& angles);
	void update_state1(C_CSGOPlayerAnimationState* state, const Vector& angles);
	void reset_state(c_baseplayeranimationstate* state);
	void copy_command(CUserCmd* cmd, int tickbase_shift);
	float get_interpolation();
	void NameSpam(bool);

	template <class T>
	static T* FindHudElement(const char* name)
	{
		static DWORD* pThis = nullptr;

		if (!pThis)
		{
			static auto pThisSignature = util::FindSignature(crypt_str("client.dll"), crypt_str("B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 5D 08"));

			if (pThisSignature)
				pThis = *reinterpret_cast<DWORD**>(pThisSignature + 0x1);
		}

		if (!pThis)
			return 0;

		static auto find_hud_element = reinterpret_cast<DWORD(__thiscall*)(void*, const char*)>(util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 53 8B 5D 08 56 57 8B F9 33 F6 39 77 28")));
		return (T*)find_hud_element(pThis, name); //-V204
	}

	template <class Type>
	static Type hook_manual(uintptr_t* vftable, uint32_t index, Type fnNew)
	{
		DWORD OldProtect;
		Type fnOld = (Type)vftable[index]; //-V108 //-V202

		VirtualProtect((void*)(vftable + index * sizeof(Type)), sizeof(Type), PAGE_EXECUTE_READWRITE, &OldProtect); //-V2001 //-V104 //-V206
		vftable[index] = (uintptr_t)fnNew; //-V108
		VirtualProtect((void*)(vftable + index * sizeof(Type)), sizeof(Type), OldProtect, &OldProtect); //-V2001 //-V104 //-V206

		return fnOld;
	}
}