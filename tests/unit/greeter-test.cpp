/*
 * Copyright 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 */

#include <tests/utils/qt-fixture.h>
#include <tests/utils/gtest-print-helpers.h>

#include <src/dbus-names.h>
#include <src/greeter.h>

#include <libqtdbustest/DBusTestRunner.h>
#include <libqtdbustest/QProcessDBusService.h>
#include <libqtdbusmock/DBusMock.h>

class GreeterFixture: public QtFixture
{
private:

    using super = QtFixture;

public:

    GreeterFixture() =default;
    ~GreeterFixture() =default;

protected:

    std::shared_ptr<QtDBusTest::DBusTestRunner> m_dbus_runner;
    std::shared_ptr<QtDBusMock::DBusMock> m_dbus_mock;
    GDBusConnection* m_bus {};

    void SetUp() override
    {
        super::SetUp();
  
        // use a fresh bus for each test run
        m_dbus_runner.reset(new QtDBusTest::DBusTestRunner());
        m_dbus_mock.reset(new QtDBusMock::DBusMock(*m_dbus_runner.get()));

        GError* error {};
        m_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, nullptr, &error);
        g_assert_no_error(error);
        g_dbus_connection_set_exit_on_close(m_bus, FALSE);
    }

    void TearDown() override
    {
        g_clear_object(&m_bus);

        super::TearDown();
    }

    void start_greeter_service(bool is_active)
    {
        // set a watcher to look for our mock greeter to appear
        bool owned {};
        QDBusServiceWatcher watcher(
            DBusNames::Greeter::NAME,
            m_dbus_runner->sessionConnection()
        );
        QObject::connect(
            &watcher,
            &QDBusServiceWatcher::serviceRegistered,
            [&owned](const QString&){owned = true;}
        );

        // start the mock greeter
        QVariantMap parameters;
        parameters["IsActive"] = QVariant(is_active);
        m_dbus_mock->registerTemplate(
            DBusNames::Greeter::NAME,
            GREETER_TEMPLATE,
            parameters,
            QDBusConnection::SessionBus
        );
        m_dbus_runner->startServices();

        // wait for the watcher
        ASSERT_TRUE(wait_for([&owned]{return owned;}));
    }
};

#define ASSERT_PROPERTY_EQ_EVENTUALLY(expected_in, property_in) \
    do { \
        const auto& e = expected_in; \
        const auto& p = property_in; \
        ASSERT_TRUE(wait_for([e, &p](){return e == p.get();})) \
            << "expected " << e << " but got " << p.get(); \
   } while(0)

/**
 * Test startup timing by looking at four different cases:
 * [unity greeter shows up on bus (before, after) we start listening]
 *   x [unity greeter is (active, inactive)]
 */

TEST_F(GreeterFixture, ActiveServiceStartsBeforeWatcher)
{
    constexpr bool is_active {true};
    constexpr Greeter::State expected {Greeter::State::ACTIVE};

    start_greeter_service(is_active);

    Greeter greeter;

    ASSERT_PROPERTY_EQ_EVENTUALLY(expected, greeter.state());
}

TEST_F(GreeterFixture, WatcherStartsBeforeActiveService)
{
    constexpr bool is_active {true};
    constexpr Greeter::State expected {Greeter::State::ACTIVE};

    Greeter greeter;

    start_greeter_service(is_active);

    ASSERT_PROPERTY_EQ_EVENTUALLY(expected, greeter.state());
}

TEST_F(GreeterFixture, InactiveServiceStartsBeforeWatcher)
{
    constexpr bool is_active {false};
    constexpr Greeter::State expected {Greeter::State::INACTIVE};

    start_greeter_service(is_active);

    Greeter greeter;

    ASSERT_PROPERTY_EQ_EVENTUALLY(expected, greeter.state());
}

TEST_F(GreeterFixture, WatcherStartsBeforeInactiveService)
{
    constexpr bool is_active {false};
    constexpr Greeter::State expected {Greeter::State::INACTIVE};

    Greeter greeter;

    start_greeter_service(is_active);

    ASSERT_PROPERTY_EQ_EVENTUALLY(expected, greeter.state());
}

