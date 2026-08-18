from gestures import Hand as TrackedHand
from models import Hand


class HandMapper:
    """
    Convert hand landmarks into a normalized hand position.

    The position is calculated from the palm landmarks,
    rather than from the bounding box.

    This makes the cursor position independent from
    finger extension and gesture shape.
    """

    #
    # Palm landmarks:
    #
    # 0  = wrist
    # 5  = index MCP
    # 9  = middle MCP
    # 13 = ring MCP
    # 17 = pinky MCP
    #

    PALM_LANDMARKS = (
        0,
        5,
        9,
        13,
        17,
    )


    def process(
        self,
        hand: TrackedHand | None,
    ) -> Hand | None:
        """
        Calculate the center of the palm.

        Returns:
            models.Hand with normalized coordinates [0, 1],
            or None if no hand is available.
        """

        if hand is None:
            return None


        if len(hand.landmarks) != 21:
            return None


        landmarks = hand.landmarks


        #
        # Calculate palm center.
        #

        x = sum(
            landmarks[index].x
            for index in self.PALM_LANDMARKS
        ) / len(self.PALM_LANDMARKS)


        y = sum(
            landmarks[index].y
            for index in self.PALM_LANDMARKS
        ) / len(self.PALM_LANDMARKS)


        #
        # Clamp to normalized coordinates.
        #

        x = max(
            0.0,
            min(1.0, x),
        )

        y = max(
            0.0,
            min(1.0, y),
        )


        #
        # Return the representation used
        # by the inference/IPC layer.
        #

        return Hand(
            x=x,
            y=y,
        )
