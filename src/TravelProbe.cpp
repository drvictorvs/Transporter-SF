#include "TravelProbe.h"

#include "Settings.h"

#include "REX/FModule.h"

namespace
{
	struct Target
	{
		const char* name;
		REL::ID     vtable;
	};

	// Vtable IDs from CommonLibSF's generated RE/IDs_VTABLE.h (Address Library IDs).
	constexpr std::size_t kTargetCount = 2;
	const std::array<Target, kTargetCount> kTargets{
		Target{ "StarMap::Util::ConfirmFastTravelCallback", RE::VTABLE::StarMap__Util__ConfirmFastTravelCallback[0] },  // map and star map
		Target{ "nsMonocle::MonocleFastTravelConfirmFunc", RE::VTABLE::nsMonocle__MonocleFastTravelConfirmFunc[0] },    // scanner
	};

	// The real signature is unknown, so the hook passes on the four register arguments and the
	// return value untouched. That is exact for functions with up to four integer or pointer
	// arguments, which is what a confirm callback is expected to take.
	using Forward = std::uintptr_t (*)(void*, std::uintptr_t, std::uintptr_t, std::uintptr_t);

	std::array<Forward, kTargetCount> g_originals{};
	std::array<int, kTargetCount>     g_calls{};

	void LogCall(std::size_t a_target, void* a_this, std::uintptr_t a_1, std::uintptr_t a_2, std::uintptr_t a_3)
	{
		REX::INFO("probe: {} call #{}: this={:#x} arg1={:#x} arg2={:#x} arg3={:#x}",
			kTargets[a_target].name, ++g_calls[a_target], reinterpret_cast<std::uintptr_t>(a_this), a_1, a_2, a_3);
		if (!a_this) {
			return;
		}
		// The first 0x40 bytes: vtable pointer, probably a reference count, then the fields that
		// should hold the destination.
		const auto* words = static_cast<const std::uint64_t*>(a_this);
		for (std::size_t i = 0; i < 8; i += 2) {
			REX::INFO("probe:   +{:02X}  {:016X}  {:016X}", i * 8, words[i], words[i + 1]);
		}
	}

	template <std::size_t I>
	std::uintptr_t Probe(void* a_this, std::uintptr_t a_1, std::uintptr_t a_2, std::uintptr_t a_3)
	{
		LogCall(I, a_this, a_1, a_2, a_3);
		return g_originals[I](a_this, a_1, a_2, a_3);
	}

	// A vtable slot that doesn't point into the game's code is past the end of the vtable
	// (or not a function), and writing it would corrupt whatever follows.
	bool PointsIntoGameCode(std::uintptr_t a_address)
	{
		const auto text = REX::FModule::GetExecutingModule().GetSection(".text");
		return a_address >= text.GetAddress() && a_address < text.GetAddress() + text.GetSize();
	}

	template <std::size_t I>
	void HookTarget(std::size_t a_slot)
	{
		const auto&                     target = kTargets[I];
		REL::Relocation<std::uintptr_t> vtable{ target.vtable };
		const auto                      entry = *reinterpret_cast<const std::uintptr_t*>(vtable.address() + sizeof(void*) * a_slot);
		if (!PointsIntoGameCode(entry)) {
			REX::WARN("probe: {} slot {} holds {:#x}, outside the game's code; not hooked", target.name, a_slot, entry);
			return;
		}
		g_originals[I] = reinterpret_cast<Forward>(vtable.write_vfunc(a_slot, &Probe<I>));
		const auto imageBase = vtable.address() - vtable.offset();
		REX::INFO("probe: hooked {} slot {} (vtable at +{:#x}, function at +{:#x})",
			target.name, a_slot, vtable.offset(), entry - imageBase);
	}
}

namespace TravelProbe
{
	void Install()
	{
		const auto mode = Settings::GetInt("TravelHook", "iMode", 0);
		if (mode != 1) {
			REX::INFO("probe: off (TravelHook iMode={})", mode);
			return;
		}
		// Slot 0 is normally the destructor; the "run" function is expected right after it.
		const auto slot = Settings::GetInt("TravelHook", "iVfuncIndex", 1);
		if (slot < 1 || slot > 8) {
			REX::WARN("probe: iVfuncIndex={} is outside 1-8; not hooked", slot);
			return;
		}
		HookTarget<0>(static_cast<std::size_t>(slot));
		HookTarget<1>(static_cast<std::size_t>(slot));
	}
}
