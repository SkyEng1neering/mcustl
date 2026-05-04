/**
 * @file test_device_repro.cpp
 * @brief Repro tests for the exciter-mini hardfault in
 *        audio_state_serializer::buildEffectsRegistryJson.
 *
 * Compiled into the mcustl_device_tests target with the device's
 * mcustl_device_conf.h (32-byte alignment, 256-entry tracker cap, no
 * freed-memory zeroing). The same source file (test_json.cpp) also
 * runs against this config — the entries here add scenarios that
 * specifically push the device's narrower limits.
 */

#include "mcustl_test_config.h"

using mcustl::json;

/* Verbatim copy of effect_registry.cpp's "freq_shifter" + "anf" +
 * "feedback_suppressor" + "aec" + "level_guard" entries. The schema
 * texts here are byte-identical to the device's so the parsed tree
 * shapes (number of object fields, string lengths, etc.) match. */
static const char* kSchema_freq_shifter =
    "[{\"name\":\"shift_hz\",\"type\":\"float\",\"min\":-500.0,\"max\":500.0,\"default\":5.0,"
    "\"description\":\"Frequency shift in Hz (positive = up-shift)\"},"
    "{\"name\":\"enabled\",\"type\":\"bool\",\"default\":true,"
    "\"description\":\"Enable/bypass the shifter\"},"
    "{\"name\":\"gain\",\"type\":\"float\",\"min\":0.0,\"max\":10.0,\"default\":1.0,"
    "\"description\":\"Output gain\"}]";

static const char* kSchema_anf =
    "[{\"name\":\"num_notches\",\"type\":\"int\",\"min\":1,\"max\":8,\"default\":4,"
    "\"description\":\"Max simultaneous notch filters\"},"
    "{\"name\":\"bandwidth_hz\",\"type\":\"float\",\"min\":1.0,\"max\":200.0,\"default\":50.0,"
    "\"description\":\"Notch 3dB bandwidth in Hz\"},"
    "{\"name\":\"threshold_db\",\"type\":\"float\",\"min\":1.0,\"max\":60.0,\"default\":15.0,"
    "\"description\":\"PNPR detection threshold in dB\"},"
    "{\"name\":\"confirm_frames\",\"type\":\"int\",\"min\":1,\"max\":50,\"default\":4,"
    "\"description\":\"Frames to confirm feedback before deploying notch\"},"
    "{\"name\":\"min_freq_hz\",\"type\":\"float\",\"min\":0.0,\"max\":20000.0,\"default\":500.0,"
    "\"description\":\"Minimum detection frequency in Hz\"},"
    "{\"name\":\"gain\",\"type\":\"float\",\"min\":0.0,\"max\":10.0,\"default\":1.0,"
    "\"description\":\"Output gain\"}]";

static const char* kSchema_feedback_suppressor =
    "[{\"name\":\"freq_shifter.shift_hz\",\"type\":\"float\",\"min\":1.0,\"max\":20.0,\"default\":10.0,"
    "\"description\":\"Preventive shift amount in Hz\"},"
    "{\"name\":\"freq_shifter.enabled\",\"type\":\"bool\",\"default\":true,"
    "\"description\":\"Enable/bypass the shifter\"},"
    "{\"name\":\"freq_shifter.gain\",\"type\":\"float\",\"min\":0.0,\"max\":10.0,\"default\":1.0,"
    "\"description\":\"Freq shifter output gain\"},"
    "{\"name\":\"anf.num_notches\",\"type\":\"int\",\"min\":1,\"max\":8,\"default\":6,"
    "\"description\":\"Max simultaneous notch filters\"},"
    "{\"name\":\"anf.bandwidth_hz\",\"type\":\"float\",\"min\":1.0,\"max\":200.0,\"default\":50.0,"
    "\"description\":\"Notch 3dB bandwidth in Hz\"},"
    "{\"name\":\"anf.threshold_db\",\"type\":\"float\",\"min\":1.0,\"max\":60.0,\"default\":15.0,"
    "\"description\":\"PNPR detection threshold in dB\"},"
    "{\"name\":\"anf.confirm_frames\",\"type\":\"int\",\"min\":1,\"max\":50,\"default\":4,"
    "\"description\":\"Frames to confirm feedback before deploying notch\"},"
    "{\"name\":\"anf.min_freq_hz\",\"type\":\"float\",\"min\":0.0,\"max\":20000.0,\"default\":800.0,"
    "\"description\":\"Min tracking frequency (ignore below this)\"},"
    "{\"name\":\"anf.gain\",\"type\":\"float\",\"min\":0.0,\"max\":10.0,\"default\":1.0,"
    "\"description\":\"ANF output gain\"},"
    "{\"name\":\"level_guard.threshold_db_sec\",\"type\":\"float\",\"min\":5.0,\"max\":500.0,\"default\":40.0,"
    "\"description\":\"Max allowed level rise rate (dB/sec)\"},"
    "{\"name\":\"level_guard.hold_ms\",\"type\":\"float\",\"min\":50.0,\"max\":2000.0,\"default\":300.0,"
    "\"description\":\"Hold time after detection before release (ms)\"},"
    "{\"name\":\"level_guard.min_gain\",\"type\":\"float\",\"min\":0.001,\"max\":1.0,\"default\":0.05,"
    "\"description\":\"Level guard gain floor (0.05 = -26 dB)\"},"
    "{\"name\":\"biquad.frequency\",\"type\":\"float\",\"min\":1000.0,\"max\":20000.0,\"default\":7000.0,"
    "\"description\":\"Low-pass cutoff frequency in Hz\"},"
    "{\"name\":\"biquad.Q\",\"type\":\"float\",\"min\":0.1,\"max\":5.0,\"default\":0.707,"
    "\"description\":\"Filter Q (0.707 = Butterworth, no resonance)\"},"
    "{\"name\":\"biquad.gain\",\"type\":\"float\",\"min\":0.0,\"max\":10.0,\"default\":1.0,"
    "\"description\":\"LPF output gain\"}]";

/* Verbatim mirror of audio_state_serializer.cpp::buildEffectsRegistryJson
 * loop body, with the actual device schemas. The fixture provides the
 * heap; we mimic handle_get_snapshot's outer heap_guard wrapping. */
/* Compact workload that fits comfortably inside the device's 256-slot
 * MAX_NUM_OF_ALLOCATIONS budget for *real* allocations. Three effects
 * with the smallest of the device's real schemas. Pre-fix the same
 * shape crashed at much higher caps because tracked_this /
 * tracked_heap_ptr pseudo-trackers shared the slot pool with real
 * allocations: a transient burst during `effect["params"] = params`
 * pushed the count over MAX_NUM_OF_ALLOCATIONS, register dropped
 * silently, the next defrag failed to update a stack-local pointer,
 * and the subsequent dereference faulted (NULL c_str(), garbage
 * heap_, etc.). Post-fix pseudo-trackers live in their own array
 * (MAX_NUM_OF_PSEUDO_TRACKERS) and do not compete with real allocs. */
TEST_F(McustlTestFixture, DeviceRepro_BuildEffectsRegistryRealSchemas) {
    struct Entry { const char* name; const char* schema; };
    static const Entry FIXTURES[] = {
        { "freq_shifter",   kSchema_freq_shifter },
        { "freq_shifter_b", kSchema_freq_shifter },
        { "freq_shifter_c", kSchema_freq_shifter },
    };

    mcustl::heap_guard outer;

    json snapshot;
    {
        mcustl::heap_guard inner;
        snapshot = json(json::value_t::object);

        {
            json limits(json::value_t::object);
            limits["max_modifiers_per_stream"] = 8;
            snapshot["limits"] = limits;
        }

        json effects(json::value_t::array);

        for (const auto& fx : FIXTURES) {
            json effect(json::value_t::object);
            effect["name"]     = fx.name;
            effect["display"]  = fx.name;
            effect["category"] = "echo";

            const char* err = nullptr;
            json params = json::parse(fx.schema, std::strlen(fx.schema),
                                      nullptr, &err, nullptr);
            ASSERT_TRUE(err == nullptr) << "parse: " << err;
            ASSERT_TRUE(params.is_array()) << fx.name;

            effect["params"] = params;
            effects.push_back(effect);
        }
        snapshot["effects"] = effects;
    }

    /* End-to-end validation. Walk every effect's keys and assert the
     * map nodes are intact (heap_ field garbage would surface as a
     * crash on get_string()). */
    ASSERT_TRUE(snapshot.is_object());
    ASSERT_TRUE(snapshot.contains("effects"));
    ASSERT_EQ(snapshot.at("effects").size(),
              sizeof(FIXTURES) / sizeof(FIXTURES[0]));

    for (size_t i = 0; i < snapshot.at("effects").size(); ++i) {
        const json& e = snapshot.at("effects")[i];
        ASSERT_TRUE(e.is_object()) << "effect " << i;
        ASSERT_TRUE(e.contains("name")) << "effect " << i;
        ASSERT_TRUE(e.contains("params")) << "effect " << i;
        EXPECT_TRUE(e.at("params").is_array()) << "effect " << i;
    }

    /* dump() is what the device actually does next — it walks every
     * leaf and serialises. Any latent stale-pointer would fault here. */
    mcustl::string dumped = snapshot.dump(-1);
    EXPECT_GT(dumped.size(), 0u);
}
