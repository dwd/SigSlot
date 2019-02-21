#include <sigslot/sigslot.h>
#include <iostream>
#include <map>
#include <string>

class Source {
public:
	/*
	 * Signals are public fields within a class.
	 * The template args are a multithreading policy, followed by
	 * zero or more payload arguments.
	 */
	mutable sigslot::signal<> signal_zero;
	mutable sigslot::signal<bool> signal_bool;
	
	Source() = default;
	Source(Source &) = delete;
	Source(Source &&) = delete;

	void kerpling() {
		m_toggle = !m_toggle;
		/*
		 * To emit a signal, you can call its "emit" member with the arguments
		 * you declared for it.
		 */
		signal_bool.emit(m_toggle);
	}
	
	void boioing() const {
		/*
		 * Or, you can emit a signal by just calling it.
		 * This is particularly nice for passing as a functor somewhere.
		 */
		signal_zero();
	}
	typedef sigslot::signal<Source &, std::string const &, bool> domain_callback_t;
	domain_callback_t & callback(std::string const & domain) {
		/*
		 * Sometimes, you might want to have a callback that's
		 * safely decoupled. This is one way of doing this.
		 */
		return m_callbacks[domain];
	}
	void connect_done(std::string const & domain) {
		/*
		 * Normally one would do something in an event loop here.
		 * This example is obviously trivial.
		 */
		auto it = m_callbacks.find(domain);
		if (it != m_callbacks.end()) {
			(*it).second.emit(*this, domain, true);
			m_callbacks.erase(it); // Don't forget to erase when done.
		}
	}
private:
	bool m_toggle{true};
	std::map<std::string,domain_callback_t> m_callbacks;
};

/*
 * A class acting as a sink needs to inherit from has_slots<>.
 * The template argument (optional) is a threading policy.
 * A sink "owns" the signal connections; when it goes out of scope
 * they'll be disconnected.
 */
class Sink : public sigslot::has_slots {
public:
	Sink() = default;
	
	/*
	 * Slots are just void functions.
	 * This one takes a bool, obviously.
	 */
	void slot_bool(bool b) {
		std::cout << "Signalled bool(" << (b ? "true" : "false" ) << ")" << std::endl;
	}
	
	/*
	 * And this one is just void.
	 */
	void slot_void() {
		std::cout << "Signalled void." << std::endl;
	}

	void connected(Source & s, std::string const & domain, bool ok) {
		std::cout << "Domain " << domain << " connected" << std::endl;
	}
};

int main() {
	Source source;
	
	/*
	 * You can call unconnected signals if you want.
	 */
	source.kerpling();
	source.boioing();

	{
		Sink sink;
		using namespace std::placeholders;

		/*
		 * If you connect a signal using a pointer-to-member, it assumes
		 * you mean to call the member normally.
		 */
		source.signal_zero.connect(&sink, &Sink::slot_void);
		/*
		 * You can also connect an arbitrary functor, like something snazzy
		 * with a lambda.
		 * Note the first argument remains a has_slots<> derivative acting
		 * as the connection's owner.
		 */
		source.signal_bool.connect(&sink, [&sink](bool b) {sink.slot_bool(b);});

		/*
		 * Now those slots will be called when the signals are emitted.
		 */
		std::cout << "Bool: ";
		source.kerpling();
		std::cout << "Void: ";
		source.boioing();

		/*
		 * We can disconnect a sink from a signal in the obvious way.
		 */
		source.signal_bool.disconnect(&sink);

		std::cout << "Bool: ";
		source.kerpling();
		source.boioing();

		/*
		 * Callbacks in this model work similarly:
		 */
		source.callback("dave.cridland.net").connect(&sink, &Sink::connected);
		// Wrong domain:
		source.connect_done("cridland.im");
		// Right domain:
		source.connect_done("dave.cridland.net");

		{
			Source source2;

			/*
			 * Multiple signals can connect to the same slot.
			 */
			source2.signal_zero.connect(&sink, &Sink::slot_void);

			source2.kerpling();
			std::cout << "Void: ";
			source2.boioing();
			source.kerpling();
			std::cout << "Void: ";
			source.boioing();

			/*
			 * If a signal is destroyed, the connections will vanish cleanly.
			 */
		}

		{
			Sink sink2;

			/*
			 * The same signal can emit to multiple slots, too.
			 */
			source.signal_zero.connect(&sink2, &Sink::slot_void);

			source.kerpling();
			std::cout << "Voidx2: ";
			source.boioing();
		}

		/*
		 * When Sinks are destroyed, the signals are disconnected.
		 */
	}

	/*
	 * These are unconnected, but you can still emit them (as a noop)
	 */
	source.kerpling();
	source.boioing();

	return 0;
}
