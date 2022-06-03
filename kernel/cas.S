.global cas

cas:
    lr.w t0, (a0) # Load original value.
    bne t0, a1, fail # Doesn’t match, so fail.
    sc.w a0, a2, (a0) # Try to update.
    li a0, 1 # Set return to true.
    jr ra # Return.
fail:
    li a0, 0 # Set return to false.
    jr ra # Return.
