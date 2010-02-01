interface ETH {
	if {1 < 0} {
		# this is not true
		ip	1.2.3.4
	} else {
		# if everything works correctly,
		# this IP should get set.
		ip	10.0.0.1
	}
}

interface WLAN {
	ip	5.6.7.8
}
