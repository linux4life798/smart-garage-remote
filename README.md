# Smart Garage Door Opener Remote

This projects aims to add the "smarts" to the [Chamberlain 953EV-P2]
garage door remote, for integration with Home Assistant. This is useful when
you don't want to hardwire one of the amazing smart garage door controllers,
like in a temporary rental property, or maybe in situations where you don't
have stable enough wifi or bluetooth in the garage (the Chamberlain remote has
better range).

## The Chosen Remote

![](images/open-case.jpg)

I chose the [Chamberlain 953EV-P2] garage door remote due to it's configurable
compatibility with almost all generations of Chamberlain, LiftMaster, and
Craftsman garage door openers. It is also a major plus that one remote can
control three (possibly four) garage door openers.

Checkout the [953EV-P2 manual] for more details about which openers are
supported. There is a nice table that shows compatibility/protocol based on the
color of your garage door opener's learn button.

[Chamberlain 953EV-P2]: https://www.chamberlain.com/3-button-visor-garage-door-remote/p/G953EV-P2MC
[953EV-P2 manual]: https://cgi.widen.net/content/uc9vrtoywt/original/114A5043.pdf?u=mcyivk&download=true

## MCU and Integration

Home Assistant seems to strongly prefer ESPHome for custom GPIO pressing stuff,
so we will make this work. I chose the ESP32-C6 due to it having support for
not only WiFi and BT, but also 802.15.4, with stacks ready zigbee, thread, and
matter. So, if we don't like ESPHome, we could integrate in a zillion different
ways.
