"""Format NCCL message identities as a GOAL tag (legacy) or as separate attributes (explicit)."""

import operator

FIELD_NAMES = ("chunk_id", "channel_id", "sequence_id", "message_kind", "communicator_id")


def unsigned_component(value):
    # Trace values arrive as ints or decimal strings; floats and bools are errors.
    try:
        number = int(value) if isinstance(value, str) and value.isdecimal() else operator.index(value)
    except TypeError:
        number = None
    if number is None or isinstance(value, bool) or not 0 <= number < 1 << 64:
        raise ValueError(f"Message identity component is not an unsigned 64-bit integer: {value!r}")
    return number


def format_message_fields(chunk, channel, sequence, kind, communicator, explicit):
    values = (chunk, channel, sequence, kind, communicator)
    if explicit:
        numbers = tuple(unsigned_component(value) for value in values)
        if numbers[3] > 5:
            raise ValueError("Unknown NCCL message kind")
        attributes = "".join(f" {name} {number}" for name, number in zip(FIELD_NAMES, numbers))
        # The attributes identify the message, so the tag stays 0.
        return "0", attributes
    # Upstream's decimal tag encoding, unchanged.
    return (str(chunk) + str(channel).zfill(2) + str(sequence).zfill(4)
            + str(kind).zfill(1) + str(communicator).zfill(2)), ""

