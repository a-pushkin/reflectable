#include <gtest/gtest.h>
#include <map>
#include <strstream>
#include "reflectable/JsonSerializer.hpp"

using namespace ReflectLib;
using namespace nlohmann;

namespace {

struct TestStruct : public Reflectable<TestStruct> {
  BEGIN_REFLECTABLE_MEMBERS()
  REFLECTABLE_MEMBER(int, foo)
  REFLECTABLE_MEMBER(float, bar)
  REFLECTABLE_MEMBER((std::map<int, float>), baz)
  END_REFLECTABLE_MEMBERS()
};

struct TwoMember : public Reflectable<TwoMember> {
  static constexpr int default_foo = 42;
  static constexpr float default_bar = 1.1;

  BEGIN_REFLECTABLE_MEMBERS()
  REFLECTABLE_MEMBER(int, foo, default_foo)
  REFLECTABLE_MEMBER(float, bar, default_bar)
  REFLECTABLE_MEMBER(std::vector<TestStruct>, ss)
  REFLECTABLE_MEMBER((std::map<int, float>), baz)
  END_REFLECTABLE_MEMBERS()
};

struct StructWithArray : public Reflectable<StructWithArray> {
  BEGIN_REFLECTABLE_MEMBERS()
  REFLECTABLE_MEMBER((std::vector<TwoMember>), arr)
  END_REFLECTABLE_MEMBERS()
};

TEST(JsonSerializer, load_from_empty) {
  TwoMember config;
  json empty = json::object();

  ASSERT_TRUE(JsonSerializer::load(empty, config));

  EXPECT_EQ(config.foo, TwoMember::default_foo);
  EXPECT_EQ(config.bar, TwoMember::default_bar);
}

TEST(JsonSerializer, load_from_partial) {
  TwoMember config;
  json partial = json::object();
  constexpr int test_foo = 1;
  partial["foo"] = test_foo;

  ASSERT_TRUE(JsonSerializer::load(partial, config));

  EXPECT_EQ(config.foo, test_foo);
  EXPECT_EQ(config.bar, TwoMember::default_bar);
}

struct Nested : public Reflectable<Nested> {
  BEGIN_REFLECTABLE_MEMBERS()
  REFLECTABLE_MEMBER(TwoMember, nested)
  END_REFLECTABLE_MEMBERS()
};

TEST(JsonSerializer, load_nested) {
  Nested config;
  json partial = json::object();

  constexpr int test_foo = 1;
  partial["nested"]["foo"] = test_foo;

  ASSERT_TRUE(JsonSerializer::load(partial, config));

  EXPECT_EQ(config.nested.foo, test_foo);
  EXPECT_EQ(config.nested.bar, TwoMember::default_bar);
}

TEST(JsonSerializer, load_array) {
  StructWithArray config;
  json partial = json::parse("{\"arr\":[{\"foo\":11},{\"foo\":22}]}");

  ASSERT_TRUE(JsonSerializer::load(partial, config));

  EXPECT_EQ(config.arr.size(), 2U);
  EXPECT_EQ(config.arr[0].foo, 11);
  EXPECT_EQ(config.arr[1].foo, 22);
}

TEST(JsonSerializer, save_load) {
  TwoMember config;
  config.foo = 11;
  config.bar = 12.0;

  json j;
  JsonSerializer::save(config, j);

  TwoMember config2;
  ASSERT_TRUE(JsonSerializer::load(j, config2));

  EXPECT_EQ(config2.foo, config.foo);
  EXPECT_EQ(config2.bar, config.bar);
}

class Config final : public ReflectLib::Reflectable<Config> {
 public:
  class TelemetryClass final : public ReflectLib::Reflectable<TelemetryClass> {
   public:
    BEGIN_REFLECTABLE_MEMBERS()
    REFLECTABLE_MEMBER(std::string, name)
    REFLECTABLE_MEMBER(std::string, name_rx)
    REFLECTABLE_MEMBER(std::string, table_name)
    REFLECTABLE_MEMBER(std::string, value_name)
    REFLECTABLE_MEMBER(size_t, value_selector)
    REFLECTABLE_MEMBER((std::unordered_map<std::string, std::string>), tags)
    REFLECTABLE_MEMBER(bool, ignore, false)
    END_REFLECTABLE_MEMBERS()
  };

  class InfluxConfig final : public ReflectLib::Reflectable<InfluxConfig> {
    BEGIN_REFLECTABLE_MEMBERS()
    REFLECTABLE_MEMBER(std::string, api_uri)
    REFLECTABLE_MEMBER(std::string, api_user)
    REFLECTABLE_MEMBER(std::string, api_password)
    REFLECTABLE_MEMBER(std::string, database)
    END_REFLECTABLE_MEMBERS()
  };

  BEGIN_REFLECTABLE_MEMBERS()
  REFLECTABLE_MEMBER(size_t, worker_threads, 8)
  REFLECTABLE_MEMBER(size_t, max_concurrent_requests, 256)
  REFLECTABLE_MEMBER(std::string, listen_prefix, "*:8088")
  REFLECTABLE_MEMBER(InfluxConfig, influx)
  REFLECTABLE_MEMBER(std::vector<TelemetryClass>, classes)
  END_REFLECTABLE_MEMBERS()
};

const std::string config_str =
    "{\"classes\":[{\"name\":\"CUDA\",\"name_rx\":\"^(?P<machine>[^/]+)/"
    "cuda-(?P<gpu>[^/]+)/\\\\w+-(?P<val>[^/"
    "]+)$\",\"table_name\":\"collectd.cuda.${val}\",\"tags\":{\"gpu\":\"${gpu}"
    "\",\"machine\":\"${machine}\"},\"value_name\":\"value\",\"value_"
    "selector\":1},{\"name\":\"EDAC\",\"name_rx\":\"^(?P<machine>[^/]+)/"
    "edac-(?P<mc>[^/]+)/errors-(?P<type>[^/"
    "]+)$\",\"table_name\":\"collectd.edac.errors\",\"tags\":{\"machine\":\"${"
    "machine}\",\"mc\":\"${mc}\",\"type\":\"${type}\"},\"value_name\":"
    "\"value\",\"value_selector\":1},{\"name\":\"Collectd "
    "battery\",\"name_rx\":\"^(?P<machine>[^/]+)/collectd-battery/(?P<val>[^/"
    "]+)$\",\"table_name\":\"collectd.battery.${val}\",\"tags\":{\"machine\":"
    "\"${machine}\"},\"value_name\":\"value\",\"value_selector\":1},{\"name\":"
    "\"EdgeRouter "
    "POE\",\"name_rx\":\"^(?P<switch>[^/]+)/edgeswitch-poe/(?P<val>[^/"
    "]+)-(?P<port>.*)$\",\"table_name\":\"collectd.edgeswitch-poe.${val}\","
    "\"tags\":{\"port\":\"${port}\",\"switch\":\"${switch}\"},\"value_name\":"
    "\"value\",\"value_selector\":1},{\"name\":\"Kasa "
    "power\",\"name_rx\":\"^kasa/(?P<sensor>[^/]+)/"
    "(?P<val>.*)$\",\"table_name\":\"collectd.kasa.${val}\",\"tags\":{"
    "\"sensor\":\"${sensor}\"},\"value_name\":\"value\",\"value_selector\":1},{"
    "\"name\":\"Laptop "
    "temp\",\"name_rx\":\"^(?P<machine>[^/]+)/sensors-acpitz-virtual-0/"
    "temperature-temp(?P<temp_sensor>\\\\d+)$\",\"table_name\":\"collectd.acpi."
    "temperature\",\"tags\":{\"machine\":\"${machine}\",\"temp_sensor\":\"${"
    "temp_sensor}\"},\"value_name\":\"value\",\"value_selector\":1},{\"name\":"
    "\"Fan "
    "speed\",\"name_rx\":\"^(?P<machine>[^/]+)/sensors-thinkpad-isa-0000/"
    "fanspeed-fan(?P<fan>\\\\d+)$\",\"table_name\":\"collectd.acpi.fanspeed\","
    "\"tags\":{\"fan\":\"${fan}\",\"machine\":\"${machine}\"},\"value_name\":"
    "\"value\",\"value_selector\":1},{\"name\":\"Battery\",\"name_rx\":\"^(?P<"
    "machine>[^/]+)/battery-(?P<battery>\\\\w+)/"
    "(?P<val>.*)$\",\"table_name\":\"collectd.acpi.battery.${val}\",\"tags\":{"
    "\"battery\":\"${battery}\",\"machine\":\"${machine}\"},\"value_name\":"
    "\"value\",\"value_selector\":1},{\"name\":\"S.M.A.R.T. "
    "attributes\",\"name_rx\":\"^(?P<machine>[^/]+)/smart-(?P<disk>\\\\w+)/"
    "smart_attribute-(?P<val>.*)$\",\"table_name\":\"collectd.smart.${val}\","
    "\"tags\":{\"disk\":\"${disk}\",\"machine\":\"${machine}\"},\"value_name\":"
    "\"value\",\"value_selector\":4},{\"name\":\"S.M.A.R.T. "
    "values\",\"name_rx\":\"^(?P<machine>[^/]+)/smart-(?P<disk>\\\\w+)/"
    "smart_(?P<val>.*)$\",\"table_name\":\"collectd.smart.${val}\",\"tags\":{"
    "\"disk\":\"${disk}\",\"machine\":\"${machine}\"},\"value_name\":\"value\","
    "\"value_selector\":1},{\"name\":\"DF "
    "stats\",\"name_rx\":\"^(?P<machine>[^/]+)/df-(?P<mount>[^/]+)/"
    "df_complex-(?P<val>.*)$\",\"table_name\":\"collectd.df.${val}\",\"tags\":{"
    "\"machine\":\"${machine}\",\"mount\":\"${mount}\"},\"value_name\":"
    "\"value\",\"value_selector\":1},{\"name\":\"CPU "
    "Utilization\",\"name_rx\":\"^(?P<machine>[^/]+)/cpu-(?P<core>\\\\d+)/"
    "cpu-(?P<type>.*)$\",\"table_name\":\"collectd.cpu.usage\",\"tags\":{"
    "\"core\":\"${core}\",\"machine\":\"${machine}\",\"type\":\"${type}\"},"
    "\"value_name\":\"value\",\"value_selector\":1},{\"name\":\"CPU "
    "Temp\",\"name_rx\":\"^(?P<machine>[^/]+)/"
    "sensors-coretemp-isa-(?P<isa>\\\\w+)/"
    "temperature-temp(?P<core>\\\\d+)$\",\"table_name\":\"collectd.cpu."
    "temperature\",\"tags\":{\"core\":\"${core}\",\"isa\":\"${isa}\","
    "\"machine\":\"${machine}\"},\"value_name\":\"value\",\"value_selector\":1}"
    ",{\"name\":\"CPU Temp - "
    "Threadripper\",\"name_rx\":\"^(?P<machine>[^/]+)/"
    "sensors-k10temp-pci-(?P<pci>\\\\w+)/"
    "temperature-temp(?P<temp>\\\\d+)$\",\"table_name\":\"collectd.cpu."
    "temperature\",\"tags\":{\"machine\":\"${machine}\",\"pci\":\"${pci}\","
    "\"temp\":\"${temp}\"},\"value_name\":\"value\",\"value_selector\":1},{"
    "\"name\":\"Network "
    "Temp\",\"name_rx\":\"^(?P<machine>[^/]+)/"
    "sensors-(?P<interface>enp\\\\w+)-pci-(?P<pci>\\\\w+)/"
    "temperature-temp(?P<temp>\\\\d+)$\",\"table_name\":\"collectd.network."
    "temperature\",\"tags\":{\"core\":\"${temp}\",\"interface\":\"${interface}"
    "\",\"isa\":\"${pci}\",\"machine\":\"${machine}\"},\"value_name\":"
    "\"value\",\"value_selector\":1},{\"name\":\"NCT6798 "
    "Data\",\"name_rx\":\"^(?P<machine>[^/]+)/"
    "sensors-nct6798-isa-(?P<isa>\\\\w+)/"
    "(?P<param>\\\\w+)-(?P<instance>\\\\w+)$\",\"table_name\":\"collectd."
    "nct6798.${param}\",\"tags\":{\"instance\":\"${instance}\",\"isa\":\"${isa}"
    "\",\"machine\":\"${machine}\"},\"value_name\":\"value\",\"value_"
    "selector\":1},{\"name\":\"Network "
    "RX\",\"name_rx\":\"^(?P<machine>[^/]+)/interface-(?P<if>[^/]+)/"
    "if_(?P<val>.*)$\",\"table_name\":\"collectd.network.rx.${val}\",\"tags\":{"
    "\"interface\":\"${if}\",\"machine\":\"${machine}\"},\"value_name\":"
    "\"value\",\"value_selector\":1},{\"name\":\"Network "
    "TX\",\"name_rx\":\"^(?P<machine>[^/]+)/interface-(?P<if>[^/]+)/"
    "if_(?P<val>.*)$\",\"table_name\":\"collectd.network.tx.${val}\",\"tags\":{"
    "\"interface\":\"${if}\",\"machine\":\"${machine}\"},\"value_name\":"
    "\"value\",\"value_selector\":2},{\"name\":\"IRQ\",\"name_rx\":\"^(?P<"
    "machine>[^/]+)/irq/"
    "irq-(?P<irq>.+)$\",\"table_name\":\"collectd.irq\",\"tags\":{\"irq\":\"${"
    "irq}\",\"machine\":\"${machine}\"},\"value_name\":\"value\",\"value_"
    "selector\":1},{\"name\":\"Load\",\"name_rx\":\"^(?P<machine>[^/]+)/load/"
    "(?P<val>.+)$\",\"table_name\":\"collectd.${val}\",\"tags\":{\"machine\":"
    "\"${machine}\"},\"value_name\":\"value\",\"value_selector\":1},{\"name\":"
    "\"Uptime\",\"name_rx\":\"^(?P<machine>[^/]+)/uptime/"
    "uptime$\",\"table_name\":\"collectd.uptime\",\"tags\":{\"machine\":\"${"
    "machine}\"},\"value_name\":\"value\",\"value_selector\":1},{\"name\":"
    "\"Uptime\",\"name_rx\":\"^(?P<machine>[^/]+)/users/"
    "users$\",\"table_name\":\"collectd.users\",\"tags\":{\"machine\":\"${"
    "machine}\"},\"value_name\":\"value\",\"value_selector\":1},{\"name\":"
    "\"Memory\",\"name_rx\":\"^(?P<machine>[^/]+)/memory/"
    "memory-(?P<type>.+)$\",\"table_name\":\"collectd.memory\",\"tags\":{"
    "\"machine\":\"${machine}\",\"type\":\"${type}\"},\"value_name\":\"value\","
    "\"value_selector\":1},{\"name\":\"UPS\",\"name_rx\":\"^(?P<machine>[^/]+)/"
    "nut-serverups/"
    "(?P<val>.+)$\",\"table_name\":\"collectd.ups.${val}\",\"tags\":{"
    "\"machine\":\"${machine}\"},\"value_name\":\"value\",\"value_selector\":1}"
    ",{\"name\":\"Ping\",\"name_rx\":\"^(?P<machine>[^/]+)/ping/"
    "(?P<val>[^\\\\-]*?)-(?P<host>.*)$\",\"table_name\":\"collectd.ping.${val}"
    "\",\"tags\":{\"host\":\"${host}\",\"machine\":\"${machine}\"},\"value_"
    "name\":\"value\",\"value_selector\":1},{\"name\":\"Processes\",\"name_"
    "rx\":\"^(?P<machine>[^/]+)/processes/"
    "ps_state-(?P<state>.*)$\",\"table_name\":\"collectd.proc.ps_state\","
    "\"tags\":{\"machine\":\"${machine}\",\"state\":\"${state}\"},\"value_"
    "name\":\"value\",\"value_selector\":1},{\"name\":\"Processes - "
    "fork_rate\",\"name_rx\":\"^(?P<machine>[^/]+)/processes/"
    "fork_rate$\",\"table_name\":\"collectd.proc.fork_rate\",\"tags\":{"
    "\"machine\":\"${machine}\"},\"value_name\":\"value\",\"value_selector\":1}"
    ",{\"name\":\"Swap\",\"name_rx\":\"^(?P<machine>[^/]+)/swap/"
    "(?P<val>.+)$\",\"table_name\":\"collectd.swap.${val}\",\"tags\":{"
    "\"machine\":\"${machine}\"},\"value_name\":\"value\",\"value_selector\":1}"
    ",{\"name\":\"ZFS "
    "ARC\",\"name_rx\":\"^(?P<machine>[^/]+)/zfs_arc/"
    "(?P<val>.+)$\",\"table_name\":\"collectd.zfs_arc.${val}\",\"tags\":{"
    "\"machine\":\"${machine}\"},\"value_name\":\"value\",\"value_selector\":1}"
    ",{\"name\":\"Entropy\",\"name_rx\":\"^(?P<machine>[^/]+)/entropy/"
    "entropy$\",\"table_name\":\"collectd.df.entropy\",\"tags\":{\"machine\":"
    "\"${machine}\"},\"value_name\":\"value\",\"value_selector\":1},{\"name\":"
    "\"SNMP RX "
    "deprecated\",\"name_rx\":\"^(?P<machine>[^/]+)/snmp/"
    "if_octets-(?P<if>\\\\d+)$\",\"table_name\":\"collectd.snmp.if_octets.rx\","
    "\"tags\":{\"interface\":\"${if}\",\"machine\":\"${machine}\"},\"value_"
    "name\":\"value\",\"value_selector\":1},{\"name\":\"SNMP TX "
    "deprecated\",\"name_rx\":\"^(?P<machine>[^/]+)/snmp/"
    "if_octets-(?P<if>\\\\d+)$\",\"table_name\":\"collectd.snmp.if_octets.tx\","
    "\"tags\":{\"interface\":\"${if}\",\"machine\":\"${machine}\"},\"value_"
    "name\":\"value\",\"value_selector\":2},{\"name\":\"SNMP "
    "RX\",\"name_rx\":\"^(?P<machine>[^/]+)/snmp/"
    "if_octets-traffic(?P<if>.*)$\",\"table_name\":\"collectd.snmp.if_octets."
    "rx\",\"tags\":{\"interface\":\"${if}\",\"machine\":\"${machine}\"},"
    "\"value_name\":\"value\",\"value_selector\":1},{\"name\":\"SNMP "
    "TX\",\"name_rx\":\"^(?P<machine>[^/]+)/snmp/"
    "if_octets-traffic(?P<if>.*)$\",\"table_name\":\"collectd.snmp.if_octets."
    "tx\",\"tags\":{\"interface\":\"${if}\",\"machine\":\"${machine}\"},"
    "\"value_name\":\"value\",\"value_selector\":2},{\"name\":\"SNMP "
    "Packets\",\"name_rx\":\"^(?P<machine>[^/]+)/snmp/"
    "derive-packets_(?P<kind>[^-]+)-(?P<if>.*)$\",\"table_name\":\"collectd."
    "snmp.packets\",\"tags\":{\"interface\":\"${if}\",\"kind\":\"${kind}\","
    "\"machine\":\"${machine}\"},\"value_name\":\"value\",\"value_selector\":1}"
    ",{\"name\":\"Disk "
    "stats\",\"name_rx\":\"^(?P<machine>[^/]+)/disk-(?P<disk>[^/]+)/"
    "(?P<val>.+)$\",\"table_name\":\"collectd.disk.${val}\",\"tags\":{\"disk\":"
    "\"${disk}\",\"machine\":\"${machine}\"},\"value_name\":\"value\",\"value_"
    "selector\":1},{\"name\":\"Collectd\",\"name_rx\":\"^(?P<machine>[^/]+)/"
    "collectd-(?P<val1>[^/]+)/"
    "(?P<val2>.+)$\",\"table_name\":\"collectd.collectd.${val1}.${val2}\","
    "\"tags\":{\"machine\":\"${machine}\"},\"value_name\":\"value\",\"value_"
    "selector\":1},{\"name\":\"CPU Freq "
    "1\",\"name_rx\":\"^(?P<machine>[^/]+)/cpufreq-(?P<core>\\\\d+)/"
    "cpufreq$\",\"table_name\":\"collectd.cpu.freq\",\"tags\":{\"core\":\"${"
    "core}\",\"machine\":\"${machine}\"},\"value_name\":\"value\",\"value_"
    "selector\":1},{\"name\":\"CPU Freq "
    "Percent\",\"name_rx\":\"^(?P<machine>[^/]+)/cpufreq-(?P<core>\\\\d+)/"
    "percent-(?P<percent>\\\\d+)$\",\"table_name\":\"collectd.cpu.freq-"
    "percent\",\"tags\":{\"core\":\"${core}\",\"machine\":\"${machine}\","
    "\"percent\":\"${percent}\"},\"value_name\":\"value\",\"value_selector\":1}"
    ",{\"name\":\"CPU Freq "
    "Transitions\",\"name_rx\":\"^(?P<machine>[^/]+)/cpufreq-(?P<core>\\\\d+)/"
    "transitions$\",\"table_name\":\"collectd.cpu.freq-transitions\",\"tags\":{"
    "\"core\":\"${core}\",\"machine\":\"${machine}\"},\"value_name\":\"value\","
    "\"value_selector\":1},{\"name\":\"CPU Freq "
    "2\",\"name_rx\":\"^(?P<machine>[^/]+)/cpufreq/"
    "cpufreq-(?P<core>\\\\d+)$\",\"table_name\":\"collectd.cpu.freq\",\"tags\":"
    "{\"core\":\"${core}\",\"machine\":\"${machine}\"},\"value_name\":"
    "\"value\",\"value_selector\":1},{\"name\":\"TED5000\",\"name_rx\":\"^(?P<"
    "machine>[^/]+)/ted/"
    "(?P<val>\\\\w+)-(?P<mtu>\\\\d+)$\",\"table_name\":\"collectd.ted.${val}\","
    "\"tags\":{\"machine\":\"${machine}\",\"mtu\":\"${mtu}\"},\"value_name\":"
    "\"value\",\"value_selector\":1}],\"influx\":{\"api_password\":\"...\","
    "\"api_uri\":\"http://bigserver.sdf4.com:8086/"
    "\",\"api_user\":\"writer\",\"database\":\"megatrends\"},\"worker_"
    "threads\":1}";
;

TEST(Config, load) {
  json j = json::parse(config_str);
  Config cfg;
  JsonSerializer::load(j, cfg);
  EXPECT_EQ(cfg.classes.size(), 42U);
  EXPECT_EQ(cfg.classes[0].name, "CUDA");
  EXPECT_EQ(cfg.classes[1].name, "EDAC");
}

}  // namespace
