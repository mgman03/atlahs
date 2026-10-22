"""Keep NCCL message identity separate from GOAL's numeric tag in explicit mode."""

import operator

FIELD_NAMES = ("chunk_id", "channel_id", "sequence_id", "message_kind", "communicator_id")


def unsigned_component(value):
    if isinstance(value, str):
        if not value.isascii() or not value.isdecimal():
            raise ValueError("Message identity components must be unsigned integers")
        number = int(value)
    else:
        try:
            number = operator.index(value)
        except TypeError as error:
            raise ValueError("Message identity components must be unsigned integers") from error
        if isinstance(value, bool):
            raise ValueError("Message identity components must be unsigned integers")
    if number < 0 or number > (1 << 64) - 1:
        raise ValueError("Message identity components must fit unsigned 64-bit fields")
    return number


def format_message_fields(chunk, channel, sequence, kind, communicator, explicit):
    values = (chunk, channel, sequence, kind, communicator)
    if explicit:
        numbers = tuple(unsigned_component(value) for value in values)
        if numbers[3] > 5:
            raise ValueError("Unknown NCCL message kind")
        attributes = "".join(f" {name} {number}" for name, number in zip(FIELD_NAMES, numbers))
        # Matching uses the explicit tuple; the standard tag carries no metadata.
        return "0", attributes
    # Preserve upstream output byte for byte for the legacy converter/scheduler.
    return (str(chunk) + str(channel).zfill(2) + str(sequence).zfill(4)
            + str(kind).zfill(1) + str(communicator).zfill(2)), ""


def calculation_message_fields(explicit):
    # Calculation/fence nodes do not participate in message matching.
    return format_message_fields(0, 0, 0, 0, 0, True)[1] if explicit else ""
