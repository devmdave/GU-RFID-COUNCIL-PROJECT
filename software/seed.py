import os
from app import app, db, Card

def seed():
    dummy_members = [
        {
            "number": "1234567890",
            "name": "Aarav Sharma",
            "enrollment": "1234567890",
            "role": "Member",
            "committee": "Technical",
            "active": True
        },
        {
            "number": "07984253060",
            "name": "Riya Patel",
            "enrollment": "07984253060",
            "role": "Volunteer",
            "committee": "Events",
            "active": True
        }
    ]

    with app.app_context():
        for member_data in dummy_members:
            existing_card = Card.query.filter_by(number=member_data["number"]).first()
            
            if existing_card:
                existing_card.name = member_data["name"]
                existing_card.enrollment = member_data["enrollment"]
                existing_card.role = member_data["role"]
                existing_card.committee = member_data["committee"]
                existing_card.active = member_data["active"]
                print(f"Updated existing card '{member_data['number']}'.")
            else:
                new_card = Card(**member_data)
                db.session.add(new_card)
                print(f"Inserted new card '{member_data['number']}'.")
                
        db.session.commit()
        print("RFID member seeding completed successfully.")

if __name__ == '__main__':
    seed()
