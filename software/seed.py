import os
from app import app, db, Card

def seed():
    with app.app_context():
        # Check if it already exists to prevent duplication errors
        existing_card = Card.query.filter_by(number="1234567890").first()
        if not existing_card:
            test_card = Card(number="1234567890", active=True)
            db.session.add(test_card)
            db.session.commit()
            print("Test card '1234567890' inserted successfully.")
        else:
            print("Test card '1234567890' already exists.")

if __name__ == '__main__':
    seed()
