main:
	$(CC) -c $(CFLAGS) bluetooth.c hci.c sdp.c self_repr.c
	$(CC) -static $(CFLAGS) apply_profile.c bluetooth.o hci.o sdp.o self_repr.o -o bmouse
	
.PHONY: clean

clean:
	rm -v bluetooth.o hci.o sdp.o self_repr.o bmouse



